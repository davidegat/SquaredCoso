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

// Colori sfumatura sole (dal più esterno al più interno)
static const uint16_t SUN_GLOW1 = 0x4200;  // arancione molto scuro/debole
static const uint16_t SUN_GLOW2 = 0x6320;  // arancione scuro
static const uint16_t SUN_GLOW3 = 0x8440;  // arancione medio
static const uint16_t SUN_GLOW4 = 0xC5A0;  // giallo-arancio
static const uint16_t SUN_GLOW5 = 0xE6C0;  // giallo chiaro

static constexpr double WO_E = TWO_PI/(365.25*86400.0);
static const float TILT      = 23.44f * DEG_TO_RAD;

// ---------------------------------------------------------------------------
// Stato locale (rimane per eventuali usi futuri)
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
// Funzioni base astronomiche (le lascio, possono servirti altrove)
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
// Cache astronomica (non più usata per la posizione della Terra, ma tenuta)
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
// Utility collisioni label - MIGLIORATO
// ---------------------------------------------------------------------------
static bool rectOverlap(int x1,int y1,int w1,int h1,
                        int x2,int y2,int w2,int h2)
{
  return !(x1+w1 < x2 || x2+w2 < x1 || y1+h1 < y2 || y2+h2 < y1);
}

// Verifica se un rettangolo interseca un cerchio
static bool rectCircleOverlap(int rx, int ry, int rw, int rh,
                               float cx, float cy, float cr)
{
  // Trova il punto del rettangolo più vicino al centro del cerchio
  float closestX = max((float)rx, min(cx, (float)(rx + rw)));
  float closestY = max((float)ry, min(cy, (float)(ry + rh)));
  
  float dx = cx - closestX;
  float dy = cy - closestY;
  
  return (dx*dx + dy*dy) < (cr * cr);
}

// Spinge la label lontano da un cerchio (Terra o Sole)
static void avoidCircle(int &lx, int &ly, int lw, int lh,
                        float cx, float cy, float cr, float margin)
{
  // Centro della label
  float lcx = lx + lw * 0.5f;
  float lcy = ly + lh * 0.5f;
  
  // Vettore dal centro del cerchio al centro della label
  float dx = lcx - cx;
  float dy = lcy - cy;
  float dist = sqrtf(dx*dx + dy*dy);
  
  // Distanza minima richiesta
  float minDist = cr + max(lw, lh) * 0.5f + margin;
  
  if(dist < minDist && dist > 0.1f) {
    float push = (minDist - dist);
    lx += (int)(dx / dist * push);
    ly += (int)(dy / dist * push);
  }
}

// Mantiene la label dentro i limiti dello schermo
static void clampToScreen(int &lx, int &ly, int lw, int lh,
                          int screenW, int screenH, int minY)
{
  const int MARGIN = 4;
  
  if(lx < MARGIN) lx = MARGIN;
  if(ly < minY + MARGIN) ly = minY + MARGIN;
  if(lx + lw > screenW - MARGIN) lx = screenW - MARGIN - lw;
  if(ly + lh > screenH - MARGIN) ly = screenH - MARGIN - lh;
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
  
  // Calcola quanto overlap c'è e spingi di conseguenza
  float overlapX = (aw + bw) * 0.5f - fabsf(acx - bcx);
  float overlapY = (ah + bh) * 0.5f - fabsf(acy - bcy);
  float k = max(overlapX, overlapY) * 0.6f / dist;
  
  ax += (int)(dx*k);
  ay += (int)(dy*k);
  bx -= (int)(dx*k);
  by -= (int)(dy*k);
}

static void resolveLabelConstraints(
  int &t1x,int &t1y,int t1w,int t1h,     // Terra
  int &s1x,int &s1y,int s1w,int s1h,     // Sole
  int &s2x,int &s2y,int s2w,int s2h,     // Stagione 1 (Estate)
  int &s3x,int &s3y,int s3w,int s3h,     // Stagione 2 (Autunno)
  int &s4x,int &s4y,int s4w,int s4h,     // Stagione 3 (Inverno)
  int &s5x,int &s5y,int s5w,int s5h,     // Stagione 4 (Primavera)
  float ex,float ey,int er,              // Terra posiz.
  float sx_c,float sy_c,int sr,          // Sole posiz.
  int screenW, int screenH, int minY     // Limiti schermo
)
{
    const float EARTH_MARGIN = 8.0f;
    const float SUN_MARGIN = 12.0f;  // Margine più grande per il sole con sfumatura
    
    for(int i=0; i<30; i++)
    {
        // Evita Terra (tutti tranne label Terra stessa che deve stare vicina)
        avoidCircle(s1x,s1y,s1w,s1h, ex,ey,er, EARTH_MARGIN);
        avoidCircle(s2x,s2y,s2w,s2h, ex,ey,er, EARTH_MARGIN);
        avoidCircle(s3x,s3y,s3w,s3h, ex,ey,er, EARTH_MARGIN);
        avoidCircle(s4x,s4y,s4w,s4h, ex,ey,er, EARTH_MARGIN);
        avoidCircle(s5x,s5y,s5w,s5h, ex,ey,er, EARTH_MARGIN);

        // Evita Sole (inclusa sfumatura)
        float sunRadius = sr + 10;  // Include la sfumatura
        avoidCircle(t1x,t1y,t1w,t1h, sx_c,sy_c,sunRadius, SUN_MARGIN);
        avoidCircle(s2x,s2y,s2w,s2h, sx_c,sy_c,sunRadius, SUN_MARGIN);
        avoidCircle(s3x,s3y,s3w,s3h, sx_c,sy_c,sunRadius, SUN_MARGIN);
        avoidCircle(s4x,s4y,s4w,s4h, sx_c,sy_c,sunRadius, SUN_MARGIN);
        avoidCircle(s5x,s5y,s5w,s5h, sx_c,sy_c,sunRadius, SUN_MARGIN);
        // Label "Sole" può stare vicino al sole

        // Collisioni tra tutte le label
        pushApart(t1x,t1y,t1w,t1h, s1x,s1y,s1w,s1h);
        pushApart(t1x,t1y,t1w,t1h, s2x,s2y,s2w,s2h);
        pushApart(t1x,t1y,t1w,t1h, s3x,s3y,s3w,s3h);
        pushApart(t1x,t1y,t1w,t1h, s4x,s4y,s4w,s4h);
        pushApart(t1x,t1y,t1w,t1h, s5x,s5y,s5w,s5h);

        pushApart(s1x,s1y,s1w,s1h, s2x,s2y,s2w,s2h);
        pushApart(s1x,s1y,s1w,s1h, s3x,s3y,s3w,s3h);
        pushApart(s1x,s1y,s1w,s1h, s4x,s4y,s4w,s4h);
        pushApart(s1x,s1y,s1w,s1h, s5x,s5y,s5w,s5h);
        pushApart(s2x,s2y,s2w,s2h, s3x,s3y,s3w,s3h);
        pushApart(s2x,s2y,s2w,s2h, s4x,s4y,s4w,s4h);
        pushApart(s2x,s2y,s2w,s2h, s5x,s5y,s5w,s5h);
        pushApart(s3x,s3y,s3w,s3h, s4x,s4y,s4w,s4h);
        pushApart(s3x,s3y,s3w,s3h, s5x,s5y,s5w,s5h);
        pushApart(s4x,s4y,s4w,s4h, s5x,s5y,s5w,s5h);

        // Mantieni dentro lo schermo
        clampToScreen(t1x,t1y,t1w,t1h, screenW,screenH,minY);
        clampToScreen(s1x,s1y,s1w,s1h, screenW,screenH,minY);
        clampToScreen(s2x,s2y,s2w,s2h, screenW,screenH,minY);
        clampToScreen(s3x,s3y,s3w,s3h, screenW,screenH,minY);
        clampToScreen(s4x,s4y,s4w,s4h, screenW,screenH,minY);
        clampToScreen(s5x,s5y,s5w,s5h, screenW,screenH,minY);
    }
}

// ---------------------------------------------------------------------------
// Sfumatura Sole
// ---------------------------------------------------------------------------
static void drawSunGlow(int cx, int cy, int sunR)
{
  // Disegna cerchi concentrici dal più esterno al più interno
  // per creare un effetto sfumatura/alone
  
  // Alone esterno molto tenue
  for(int r = sunR + 10; r > sunR + 8; r--) {
    gfx->drawCircle(cx, cy, r, SUN_GLOW1);
  }
  
  // Secondo livello
  for(int r = sunR + 8; r > sunR + 6; r--) {
    gfx->drawCircle(cx, cy, r, SUN_GLOW2);
  }
  
  // Terzo livello
  for(int r = sunR + 6; r > sunR + 4; r--) {
    gfx->drawCircle(cx, cy, r, SUN_GLOW3);
  }
  
  // Quarto livello
  for(int r = sunR + 4; r > sunR + 2; r--) {
    gfx->drawCircle(cx, cy, r, SUN_GLOW4);
  }
  
  // Livello più interno, quasi giallo sole
  for(int r = sunR + 2; r > sunR; r--) {
    gfx->drawCircle(cx, cy, r, SUN_GLOW5);
  }
}

// ---------------------------------------------------------------------------
// Terra 
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

  // 1) pixel illuminati
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

  // 2) 30% di pixel verdi (era 15%)
  int targetGreen = (illumCount * 90) / 100;
  if(targetGreen < 0) targetGreen = 0;
  if(targetGreen > illumCount) targetGreen = illumCount;

  uint8_t flag[MAX_PIX];
  for(int i=0; i<illumCount; i++)
    flag[i] = (i < targetGreen) ? 1 : 0;

  for(int i=illumCount-1; i>0; i--){
    int j = random(0, i+1);
    uint8_t t = flag[i];
    flag[i] = flag[j];
    flag[j] = t;
  }

  // 3) disegno finale
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

        if(idx < illumCount && flag[idx]){
          c = EAR_GREEN;
          isGreen = true;
        }

        if(!isGreen && dot > 0.4f)
          c |= 0x4208;

        idx++;
      }
      else {
        c &= 0x7BEF;
      }

      gfx->drawPixel(ex + dx, ey + dy, c);
    }
  }
}

// ===========================================================================
// PAGE
// ===========================================================================
void pageStellar(){
  // ---------------------------------------
  // Setup page
  // ---------------------------------------
  drawHeader((g_lang=="it")?"Sistema Solare":"Solar system");

  int w = gfx->width();
  int h = gfx->height();

  gfx->fillRect(0, PAGE_Y, w, h-PAGE_Y, BG);

  initStars();
  drawStars();

  int cx = w/2;
  int cy = PAGE_Y + (h-PAGE_Y)/2 + 10;

  // ---------------------------------------
  // ORBITA CIRCOLARE
  // ---------------------------------------
  float r = min(w, h-PAGE_Y) * 0.40f;

  for(int d=0; d<360; d++){
    float ang = d * DEG_TO_RAD;
    gfx->drawPixel(cx + r*cosf(ang),
                   cy + r*sinf(ang),
                   COL_DIVIDER);
  }

  // ---------------------------------------
  // Punti stagionali sulla circonferenza
  // ---------------------------------------
  const char* S1s=(g_lang=="it")?"Sol. Estate":"Summer Sol.";
  const char* S2s=(g_lang=="it")?"Eq. Autunno":"Autumn Eq.";
  const char* S3s=(g_lang=="it")?"Sol. Inverno":"Winter Sol.";
  const char* S4s=(g_lang=="it")?"Eq. Primavera":"Spring Eq.";

  const float D_SPRING = 79.0f;
  const float D_SUMMER = 172.0f;
  const float D_AUTUMN = 266.0f;
  const float D_WINTER = 355.0f;

  const float F1 = 79.0f / 365.25f;
  const float F2 = 172.0f / 365.25f;
  const float F3 = 266.0f / 365.25f;
  const float F4 = 355.0f / 365.25f;

  const float ps = -3.0f / 365.25f;

  auto orbitPos=[&](float f,int &x,int &y){
    float p=f+ps;
    p -= floorf(p);
    float ang=p*TWO_PI;
    x = cx + (int)(r*cosf(ang));
    y = cy + (int)(r*sinf(ang));
  };

  int xS1,yS1, xS2,yS2, xS3,yS3, xS4,yS4;
  orbitPos(F1,xS1,yS1);
  orbitPos(F2,xS2,yS2);
  orbitPos(F3,xS3,yS3);
  orbitPos(F4,xS4,yS4);

auto drawTick = [&](int px, int py, int len){
  float dx = px - cx;
  float dy = py - cy;
  float dist = sqrtf(dx*dx + dy*dy);
  if(dist < 1) dist = 1;
  float nx = dx / dist;
  float ny = dy / dist;
  int x1 = px - (int)(nx * len);
  int y1 = py - (int)(ny * len);
  int x2 = px + (int)(nx * len);
  int y2 = py + (int)(ny * len);
  gfx->drawLine(x1, y1, x2, y2, COL_DIVIDER);
};

drawTick(xS1, yS1, 6);
drawTick(xS2, yS2, 6);
drawTick(xS3, yS3, 6);
drawTick(xS4, yS4, 6);

  // ---------------------------------------
  // Sole con sfumatura
  // ---------------------------------------
  int scx = cx, scy = cy;
  int sunR = 26;
  
  // Prima disegna la sfumatura
  drawSunGlow(scx, scy, sunR);
  
  // Poi il sole pieno
  gfx->fillCircle(scx, scy, sunR, SUN);
  gfx->drawCircle(scx, scy, sunR + 1, COL_DIVIDER);

  // ---------------------------------------
  // CALCOLO ANGOLO STAGIONI
  // ---------------------------------------
  auto normAng = [](float a)->float{
    while(a < 0)      a += TWO_PI;
    while(a >= TWO_PI)a -= TWO_PI;
    return a;
  };

  float angSUM = normAng(atan2f((float)(yS1 - cy), (float)(xS1 - cx)));
  float angAUT = normAng(atan2f((float)(yS2 - cy), (float)(xS2 - cx)));
  float angWIN = normAng(atan2f((float)(yS3 - cy), (float)(xS3 - cx)));
  float angSPR = normAng(atan2f((float)(yS4 - cy), (float)(xS4 - cx)));

  // ---------------------------------------
  // GIORNO DELL'ANNO
  // ---------------------------------------
  time_t now; 
  time(&now);
  struct tm lt;
  localtime_r(&now, &lt);

  int year = lt.tm_year + 1900;
  bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
  int daysInYear = leap ? 366 : 365;

  float day = (float)lt.tm_yday;

  // ---------------------------------------
  // KNOTS
  // ---------------------------------------
  const int K = 5;
  float kd[K] = {
    D_WINTER - (float)daysInYear,
    D_SPRING,
    D_SUMMER,
    D_AUTUMN,
    D_WINTER
  };

  float ka[K] = {
    angWIN - TWO_PI,
    angSPR,
    angSUM,
    angAUT,
    angWIN
  };

  float angEarth = angWIN;
  for(int i=0; i < K-1; i++){
    if(day >= kd[i] && day <= kd[i+1]){
      float t = (day - kd[i]) / (kd[i+1] - kd[i]);
      angEarth = ka[i] + t * (ka[i+1] - ka[i]);
      break;
    }
  }

  angEarth = normAng(angEarth);

  // ---------------------------------------
  // TERRA
  // ---------------------------------------
  float ex = cx + r * cosf(angEarth);
  float ey = cy + r * sinf(angEarth);

  int er = 15;

  float lx = scx - ex;
  float ly = scy - ey;
  float ll = hypotf(lx, ly);
  if(ll < 1.0f) ll = 1.0f;
  lx /= ll;
  ly /= ll;

  drawEarth(ex, ey, er, lx, ly, 0, true);

  // ---------------------------------------
  // PREPARAZIONE LABEL
  // ---------------------------------------
  gfx->setTextSize(1);
  gfx->setTextColor(COL_TEXT, BG);

  const char* terra = (g_lang=="it") ? "Terra" : "Earth";
  const char* sole  = (g_lang=="it") ? "Sole"  : "Sun";

  // Terra label - posizionata radialmente verso l'esterno
  int tew=6*strlen(terra), teh=8;
  float earthOutX = ex - scx;
  float earthOutY = ey - scy;
  float earthOutLen = sqrtf(earthOutX*earthOutX + earthOutY*earthOutY);
  if(earthOutLen < 1) earthOutLen = 1;
  int tex = (int)(ex + (earthOutX/earthOutLen) * (er + 8));
  int tey = (int)(ey + (earthOutY/earthOutLen) * (er + 8) - teh/2);

  // Sole label - posizionata a destra del sole
  int sew=6*strlen(sole), seh=8;
  int sex = scx + sunR + 14;  // Dopo la sfumatura
  int sey = scy - seh/2;

  // Stagioni - posizionate radialmente verso l'esterno
  int l1w=6*strlen(S1s), l1h=8;
  int l2w=6*strlen(S2s), l2h=8;
  int l3w=6*strlen(S3s), l3h=8;
  int l4w=6*strlen(S4s), l4h=8;

  // Calcola direzione radiale per ogni stagione
  auto radialLabel = [&](int px, int py, int lw, int lh, int &lx, int &ly){
    float dx = px - cx;
    float dy = py - cy;
    float len = sqrtf(dx*dx + dy*dy);
    if(len < 1) len = 1;
    // Posiziona la label verso l'esterno
    lx = px + (int)((dx/len) * 16) - lw/2;
    ly = py + (int)((dy/len) * 16) - lh/2;
  };

  int l1x, l1y, l2x, l2y, l3x, l3y, l4x, l4y;
  radialLabel(xS1, yS1, l1w, l1h, l1x, l1y);
  radialLabel(xS2, yS2, l2w, l2h, l2x, l2y);
  radialLabel(xS3, yS3, l3w, l3h, l3x, l3y);
  radialLabel(xS4, yS4, l4w, l4h, l4x, l4y);

  // ---------------------------------------
  // RISOLVI COLLISIONI LABEL
  // ---------------------------------------
  resolveLabelConstraints(
      tex, tey, tew, teh,       // Terra
      sex, sey, sew, seh,       // Sole
      l1x, l1y, l1w, l1h,       // Estate
      l2x, l2y, l2w, l2h,       // Autunno
      l3x, l3y, l3w, l3h,       // Inverno
      l4x, l4y, l4w, l4h,       // Primavera
      ex, ey, er,               // posizione Terra
      (float)scx, (float)scy, sunR,  // posizione Sole
      w, h, PAGE_Y              // limiti schermo
  );

  // ---------------------------------------
  // DISEGNA LABEL
  // ---------------------------------------
  gfx->setCursor(sex, sey); gfx->print(sole);
  gfx->setCursor(tex, tey); gfx->print(terra);

  gfx->setCursor(l1x, l1y); gfx->print(S1s);
  gfx->setCursor(l2x, l2y); gfx->print(S2s);
  gfx->setCursor(l3x, l3y); gfx->print(S3s);
  gfx->setCursor(l4x, l4y); gfx->print(S4s);

  gfx->setTextSize(TEXT_SCALE);
}
