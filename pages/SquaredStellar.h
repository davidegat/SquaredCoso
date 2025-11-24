#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "../handlers/globals.h"

// ---------------------------------------------------------------------------
// Risorse esterne
// ---------------------------------------------------------------------------
extern Arduino_RGB_Display* gfx;
extern const uint16_t COL_TEXT, COL_ACCENT1, COL_ACCENT2, COL_DIVIDER;
extern const int PAGE_X, PAGE_Y, PAGE_W, PAGE_H, TEXT_SCALE;
extern String g_lang;
extern void drawHeader(const String&);

// ---------------------------------------------------------------------------
// Costanti colore e parametri fisici
// ---------------------------------------------------------------------------
static const uint16_t BG       = 0x0000;
static const uint16_t SUN      = 0xFFE0;
static const uint16_t EAR_BLUE = 0x07FF;
static const uint16_t EAR_GREEN= 0x07E0;
static const uint16_t S1       = 0x8410;
static const uint16_t S2       = 0xC618;
static const uint16_t S3       = 0xFFFF;
static const uint16_t AX       = 0xF800;

static constexpr double WO_E = TWO_PI/(365.25*86400.0);
static const float TILT      = 23.44f * DEG_TO_RAD;

// ---------------------------------------------------------------------------
// Stato locale
// ---------------------------------------------------------------------------
static bool   ok   = false;
static float  refE = 0;
static time_t refT = 0;

// ---------------------------------------------------------------------------
// Stelle
// ---------------------------------------------------------------------------
static uint32_t nextPulse = 0;
static const int NS        = 80;
static int sx[NS], sy[NS];
static uint8_t sp[NS];

// ---------------------------------------------------------------------------
// Funzioni base
// ---------------------------------------------------------------------------
static float n360(float a){
  while(a < 0)   a += 360;
  while(a >= 360)a -= 360;
  return a;
}

static double jd(time_t t){
  struct tm tm;
  gmtime_r(&t,&tm);
  int y = tm.tm_year + 1900;
  int m = tm.tm_mon  + 1;
  int D = tm.tm_mday;
  int A = (14-m)/12;
  y += 4800 - A;
  m += 12*A - 3;

  return D + (153*m+2)/5 + 365*y + y/4 - y/100 + y/400 - 32045
       + (tm.tm_hour-12)/24.0 + tm.tm_min/1440.0 + tm.tm_sec/86400.0;
}

static float sunLon(time_t t){
  double JD = jd(t);
  double T  = (JD - 2451545.0)/36525.0;
  double L0 = n360(280.46646 + 36000.77*T);
  double M  = (357.52911 + 35999.05*T)*DEG_TO_RAD;
  double L  = L0 + 1.914602*sin(M) + 0.019993*sin(2*M) + 0.000289*sin(3*M);
  return n360(L);
}

// ---------------------------------------------------------------------------
bool fetchStellar(){
  time_t now; time(&now);
  refE = sunLon(now)*DEG_TO_RAD;
  refT = now;
  ok   = true;
  return true;
}

static void getNow(float &earth){
  if(!ok || !refT) fetchStellar();

  time_t now; time(&now);
  double dt = difftime(now,refT);
  if(dt < 0)     dt = 0;
  if(dt > 3600)  dt = 3600;

  earth = refE + WO_E*dt;
  if(earth >= TWO_PI) earth -= TWO_PI;
}

// ---------------------------------------------------------------------------
// Stelle
// ---------------------------------------------------------------------------
static void initStars(){
  time_t n; time(&n);
  randomSeed((uint32_t)n ^ 0x12345678);
  int w = gfx->width();
  int h = gfx->height();
  for(int i=0;i<NS;i++){
    sx[i] = random(0,w);
    sy[i] = random(PAGE_Y,h);
    sp[i] = random(0,3);
  }
}

static void drawStars(){
  for(int i=0;i<NS;i++){
    uint16_t c = (sp[i]==0)?S1 : (sp[i]==1)?S2 : S3;
    gfx->drawPixel(sx[i],sy[i],c);
  }
}

void tickStellar(){
  uint32_t t = millis();
  if(t < nextPulse) return;
  nextPulse = t + 1000;
  for(int i=0;i<NS;i++) sp[i] = (sp[i]+1)%3;
  drawStars();
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
static bool rectOverlap(int x1,int y1,int w1,int h1,
                        int x2,int y2,int w2,int h2)
{
  return !(x1+w1 < x2 || x2+w2 < x1 || y1+h1 < y2 || y2+h2 < y1);
}

static void avoidTerraHit(int &lx,int &ly,int lw,int lh,
                          float ex,float ey,int er)
{
  float cx = lx + lw*0.5f;
  float cy = ly + lh*0.5f;
  float dx = cx - ex;
  float dy = cy - ey;
  float dist = sqrtf(dx*dx+dy*dy);
  float minD = er + lw*0.7f + 12;  // distacco aumentato
  if(dist >= minD) return;
  if(dist < 1){ dx=1; dy=0; dist=1; }
  float k = (minD - dist)/dist;
  lx += dx*k;
  ly += dy*k;
}

static void pushApart(int &ax,int &ay,int aw,int ah,
                      int &bx,int &by,int bw,int bh)
{
  if(!rectOverlap(ax,ay,aw,ah,bx,by,bw,bh)) return;
  float acx=ax+aw*0.5f, acy=ay+ah*0.5f;
  float bcx=bx+bw*0.5f, bcy=by+bh*0.5f;
  float dx=acx-bcx, dy=acy-bcy;
  float dist=sqrtf(dx*dx+dy*dy);
  if(dist<1){ dx=1; dy=0; dist=1; }
  float k=6/dist;   // maggiore distacco
  ax += dx*k;
  ay += dy*k;
  bx -= dx*k;
  by -= dy*k;
}

static void resolveLabelConstraints(
  int &t1x,int &t1y,int t1w,int t1h,
  int &t2x,int &t2y,int t2w,int t2h,
  int &s1x,int &s1y,int s1w,int s1h,
  int &s2x,int &s2y,int s2w,int s2h,
  int &s3x,int &s3y,int s3w,int s3h,
  int &s4x,int &s4y,int s4w,int s4h,
  float ex1,float ey1,int er1,
  float ex2,float ey2,int er2)
{
  for(int i=0;i<22;i++){
    avoidTerraHit(t1x,t1y,t1w,t1h,ex1,ey1,er1);
    avoidTerraHit(t1x,t1y,t1w,t1h,ex2,ey2,er2);
    avoidTerraHit(t2x,t2y,t2w,t2h,ex1,ey1,er1);
    avoidTerraHit(t2x,t2y,t2w,t2h,ex2,ey2,er2);

    avoidTerraHit(s1x,s1y,s1w,s1h,ex1,ey1,er1);
    avoidTerraHit(s2x,s2y,s2w,s2h,ex1,ey1,er1);
    avoidTerraHit(s3x,s3y,s3w,s3h,ex1,ey1,er1);
    avoidTerraHit(s4x,s4y,s4w,s4h,ex1,ey1,er1);

    pushApart(t1x,t1y,t1w,t1h,s1x,s1y,s1w,s1h);
    pushApart(t1x,t1y,t1w,t1h,s2x,s2y,s2w,s2h);
    pushApart(t1x,t1y,t1w,t1h,s3x,s3y,s3w,s3h);
    pushApart(t1x,t1y,t1w,t1h,s4x,s4y,s4w,s4h);

    pushApart(t2x,t2y,t2w,t2h,s1x,s1y,s1w,s1h);
    pushApart(t2x,t2y,t2w,t2h,s2x,s2y,s2w,s2h);
    pushApart(t2x,t2y,t2w,t2h,s3x,s3y,s3w,s3h);
    pushApart(t2x,t2y,t2w,t2h,s4x,s4y,s4w,s4h);

    pushApart(s1x,s1y,s1w,s1h,s2x,s2y,s2w,s2h);
    pushApart(s1x,s1y,s1w,s1h,s3x,s3y,s3w,s3h);
    pushApart(s1x,s1y,s1w,s1h,s4x,s4y,s4w,s4h);
    pushApart(s2x,s2y,s2w,s2h,s3x,s3y,s3w,s3h);
    pushApart(s2x,s2y,s2w,s2h,s4x,s4y,s4w,s4h);
    pushApart(s3x,s3y,s3w,s3h,s4x,s4y,s4w,s4h);
  }
}

// ---------------------------------------------------------------------------
// Cerchio puntinato
// ---------------------------------------------------------------------------
static void drawDashLine(int x1,int y1,int x2,int y2){
  float dx=x2-x1, dy=y2-y1;
  float L=hypotf(dx,dy);
  float ux=dx/L, uy=dy/L;
  for(float i=0;i<L;i+=6)
    gfx->drawPixel(x1+ux*i,y1+uy*i,AX);
}

// ---------------------------------------------------------------------------
// Terra (invariata)
// ---------------------------------------------------------------------------
void drawEarth(
  float ex,float ey,int er,
  float lx,float ly,
  float seasonPhase,
  bool boreale )
{
  const int MAX_PIX=512;
  int16_t dxList[MAX_PIX], dyList[MAX_PIX];
  int illumCount=0;

  // ------------------------------------
  // 1) Raccogli tutti i pixel illuminati
  // ------------------------------------
  for(int dy=-er; dy<=er; dy++){
    for(int dx=-er; dx<=er; dx++){
      if(dx*dx + dy*dy > er*er) continue;

      float nx = dx/(float)er;
      float ny = dy/(float)er;
      float dot = nx*lx + ny*ly;

      if(dot > 0.2f && illumCount < MAX_PIX){
        dxList[illumCount] = dx;
        dyList[illumCount] = dy;
        illumCount++;
      }
    }
  }

  // -----------------------------------------
  // 2) Applica esattamente il 15% di pixel verdi
  // -----------------------------------------
  int targetGreen = (illumCount * 15) / 100;
  if(targetGreen < 0) targetGreen = 0;
  if(targetGreen > illumCount) targetGreen = illumCount;

  uint8_t flag[MAX_PIX];
  for(int i=0; i<illumCount; i++)
    flag[i] = (i < targetGreen) ? 1 : 0;

  // shuffle
  for(int i=illumCount-1; i>0; i--){
    int j = random(0, i+1);
    uint8_t t = flag[i];
    flag[i] = flag[j];
    flag[j] = t;
  }

  // -----------------------------------------
  // 3) Disegno finale pixel per pixel
  // -----------------------------------------
  int idx = 0;

  for(int dy=-er; dy<=er; dy++){
    for(int dx=-er; dx<=er; dx++){
      if(dx*dx + dy*dy > er*er)
        continue;

      float nx = dx/(float)er;
      float ny = dy/(float)er;
      float dot = nx*lx + ny*ly;

      bool lit = (dot > -0.2f);

      uint16_t c = EAR_BLUE;

      if(lit){
        bool isGreen = false;

        // verde 15%
        if(idx < illumCount && flag[idx]){
          c = EAR_GREEN;
          isGreen = true;
        }

        // highlight solo sul blu
        if(!isGreen && dot > 0.4f)
          c |= 0x4208;

        idx++;
      }
      else {
        // ombra
        c &= 0x7BEF;
      }

      gfx->drawPixel(ex + dx, ey + dy, c);
    }
  }

  // -----------------------------------------
  // 4) Asse inclinato
  // -----------------------------------------
  float base = atan2f(ly, lx) + HALF_PI;
  float angle = base + (boreale ? TILT*seasonPhase : -TILT*seasonPhase);

  int ax1 = ex + cosf(angle)*(er*2);
  int ay1 = ey + sinf(angle)*(er*2);
  int ax2 = ex - cosf(angle)*(er*2);
  int ay2 = ey - sinf(angle)*(er*2);

  gfx->drawLine(ax1, ay1, ax2, ay2, 0xFFFF);
}



// ===========================================================================
// PAGE
// ===========================================================================
void pageStellar(){
  float EA; getNow(EA);

  drawHeader((g_lang=="it")?"Sistema Solare":"Solar system");

  int w=gfx->width(), h=gfx->height();
  gfx->fillRect(0,PAGE_Y,w,h-PAGE_Y,BG);

  initStars();
  drawStars();

  int cx=w/2;
  int cy=PAGE_Y+(h-PAGE_Y)/2+10;

  // ------------------
  // ORBITA CIRCOLARE
  // ------------------
  float r = min(w,h-PAGE_Y)*0.40f;

  for(int d=0; d<360; d++){
    float ang=d*DEG_TO_RAD;
    gfx->drawPixel(cx + r*cosf(ang),
                   cy + r*sinf(ang),
                   COL_DIVIDER);
  }

  // stagioni
  const char* S1s=(g_lang=="it")?"Estate":"Summer";
  
  const char* S2s=(g_lang=="it")?"Autunno":"Autumn";
  
  const char* S3s=(g_lang=="it")?"Inverno":"Winter";
  
  const char* S4s=(g_lang=="it")?"Primavera":"Spring";

  const float F1=79.0/365.25;
  const float F2=172.0/365.25;
  const float F3=266.0/365.25;
  const float F4=355.0/365.25;

  const float ps=-3.0/365.25;

  auto spos=[&](float f,int &x,int &y){
    float p=f+ps; p-=floorf(p);
    float ang=p*TWO_PI;
    x=cx + r*cosf(ang);
    y=cy + r*sinf(ang);
  };

  int xs,ys,xa,ya,xi,yi,xp,yp;
  spos(F1,xs,ys);
  spos(F2,xa,ya);
  spos(F3,xi,yi);
  spos(F4,xp,yp);

  // Sole centrato
  int scx=cx, scy=cy;
  gfx->fillCircle(scx,scy,20,SUN);
  gfx->drawCircle(scx,scy,21,COL_DIVIDER);

  // TERRE
  float ex1=cx + r*cosf(EA);
  float ey1=cy + r*sinf(EA);
  float ex2=cx - r*cosf(EA);
  float ey2=cy - r*sinf(EA);
  int er=11;

  float lx1=scx-ex1, ly1=scy-ey1;
  float ll1=hypotf(lx1,ly1); lx1/=ll1; ly1/=ll1;

  float lx2=scx-ex2, ly2=scy-ey2;
  float ll2=hypotf(lx2,ly2); lx2/=ll2; ly2/=ll2;

  float season=cosf(EA);

  drawEarth(ex1,ey1,er,lx1,ly1,season,true);
  drawEarth(ex2,ey2,er,lx2,ly2,season,false);

  // TESTI
  gfx->setTextSize(1);
  gfx->setTextColor(COL_TEXT,BG);

  const char* terra=(g_lang=="it")?"Terra":"Earth";

  int t1w=6*strlen(terra), t1h=8;
  int t2w=t1w, t2h=8;

  int t1x=ex1+er+6, t1y=ey1-4;
  int t2x=ex2+er+6, t2y=ey2-4;

  int s1w=6*strlen(S1s), s1h=8;
  int s2w=6*strlen(S2s), s2h=8;
  int s3w=6*strlen(S3s), s3h=8;
  int s4w=6*strlen(S4s), s4h=8;

  int s1x=xs-18, s1y=ys-12;
  int s2x=xa-18, s2y=ya-12;
  int s3x=xi-18, s3y=yi+4;
  int s4x=xp+4,  s4y=yp-10;

  resolveLabelConstraints(
    t1x,t1y,t1w,t1h,
    t2x,t2y,t2w,t2h,
    s1x,s1y,s1w,s1h,
    s2x,s2y,s2w,s2h,
    s3x,s3y,s3w,s3h,
    s4x,s4y,s4w,s4h,
    ex1,ey1,er,
    ex2,ey2,er
  );

  // ---------------------------
  // ASSI STAGIONI → SEMPRE RETTI
  // ---------------------------
  drawDashLine(xs,ys,scx,scy);
  drawDashLine(scx,scy,xi,yi);

  drawDashLine(xa,ya,scx,scy);
  drawDashLine(scx,scy,xp,yp);

  // stampe
  gfx->setCursor(s1x,s1y); gfx->print(S1s);
  gfx->setCursor(s2x,s2y); gfx->print(S2s);
  gfx->setCursor(s3x,s3y); gfx->print(S3s);
  gfx->setCursor(s4x,s4y); gfx->print(S4s);

  gfx->setCursor(t1x,t1y); gfx->print(terra);
  gfx->setCursor(t2x,t2y); gfx->print(terra);

  gfx->setTextSize(TEXT_SCALE);
}

