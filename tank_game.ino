#include <LCDWIKI_GUI.h>
#include <LCDWIKI_SPI.h>
#include <math.h>

// —— Display setup ——
#define MODEL       ILI9488_18
#define CS_PIN      10
#define CD_PIN       9
#define RST_PIN      8
#define LED_PIN     -1

LCDWIKI_SPI lcd(MODEL, CS_PIN, CD_PIN, RST_PIN, LED_PIN);

// —— Screen dimensions ——
#define SCREEN_W 480
#define SCREEN_H 320

// —— Colors (RGB565) ——
#define BLACK       0x0000
#define WHITE       0xFFFF
#define BLUE        0x001F
#define GREEN       0x03A0
#define RED         0xF800
#define BROWN       0xA145
#define DARK_GREY   0x8410

// —— Tank & UI settings ——
const int numTanks   = 2;
int spawnXs[numTanks], groundYs[numTanks];
const int tankWidth  = 24;
const int tankHeight = 10;

// UI layout
const int barW      = 100;
const int barH      =   8;
const int margin    =  10;
const int topOffset =  20;  // push UI down
const int p1x       = margin;
const int p2x       = SCREEN_W - margin - barW;
const int healthY   = margin + topOffset;
const int powerY    = healthY + barH + 4;
const int uiBoxPad  =   5;  // padding around UI boxes

// —— Button pins ——
#define ANGLE_DEC_PIN 2
#define ANGLE_INC_PIN 3
#define POWER_DEC_PIN 4
#define POWER_INC_PIN 5
#define SHOOT_PIN     6

// —— State variables ——
int angle       = 90;    // 0…180 (90 = horizontal)
int prevAngle   = 90;
int power       = 50;    // 0…100
const int angleStep = 5;
const int powerStep = 2; // finer increments
bool lastAdec   = HIGH, lastAinc = HIGH;
bool lastPdec   = HIGH, lastPinc = HIGH;
bool lastShoot  = HIGH;

// background color
uint16_t backgroundColor;

// Generate terrain (unchanged) …
void generateTerrain() {
  randomSeed(analogRead(A0));
  int h = SCREEN_H/2, minH = SCREEN_H/4, maxH = SCREEN_H-20;
  lcd.Set_Draw_color(BROWN);
  const int chunkW = 5;
  int x = 0;
  while (x < SCREEN_W) {
    int run = random(5,16), step = random(1,4), dir = random(0,2)?1:-1;
    int target = constrain(h + dir*step*run, minH, maxH), startH = h;
    for (int i=0; i<run && x<SCREEN_W; i++, x+=chunkW) {
      float t = float(i)/run, s = t*t*(3-2*t);
      int hi = constrain(startH + int((target-startH)*s), 0, SCREEN_H-1);
      for (int dx=0; dx<chunkW && x+dx<SCREEN_W; dx++) {
        int px = x+dx;
        lcd.Fill_Rectangle(px, hi, px, SCREEN_H-1);
        for (int ti=0; ti<numTanks; ti++)
          if (px == spawnXs[ti]) groundYs[ti] = hi - 1;
      }
    }
    h = target;
  }
}

// Draw a trapezoid-shaped tank at (x,y) with long side on top
void drawTank(int x, int y, uint16_t topColor, uint16_t bottomColor) {
  int topW = tankWidth;
  int botW = tankWidth * 2 / 3;
  int h    = tankHeight;
  int dx   = (topW - botW) / 2;

  int x1 = x,          y1 = y - h;  // top-left
  int x2 = x + topW,   y2 = y - h;  // top-right
  int x3 = x + dx + botW, y3 = y;   // bot-right
  int x4 = x + dx,        y4 = y;   // bot-left

  // fill bottom color
  lcd.Set_Draw_color(bottomColor);
  lcd.Fill_Triangle(x1,y1, x4,y4, x3,y3);
  lcd.Fill_Triangle(x1,y1, x2,y2, x3,y3);

  // fill top third with white
  int th = h / 3;
  int midW = topW - ((topW - botW) * th) / h;
  int mdx  = (topW - midW) / 2;
  int yMid = y - h + th;
  int mx1  = x + mdx,        my1 = yMid;
  int mx2  = x + mdx + midW, my2 = yMid;
  lcd.Set_Draw_color(WHITE);
  lcd.Fill_Triangle(x1,y1, mx1,my1, x2,y2);
  lcd.Fill_Triangle(mx1,my1, mx2,my2, x2,y2);

  // outline in BLACK
  lcd.Set_Draw_color(BLACK);
  lcd.Draw_Line(x1,y1, x2,y2);
  lcd.Draw_Line(x2,y2, x3,y3);
  lcd.Draw_Line(x3,y3, x4,y4);
  lcd.Draw_Line(x4,y4, x1,y1);
}

// Draw small tank icon next to UI (bottomColor only)
void drawTankIcon(int x, int y, uint16_t bottomColor) {
  const int w = 20, h = 10;
  int topW = w;
  int botW = w * 2 / 3;
  int dx   = (topW - botW) / 2;

  int x1 = x,          y1 = y - h;
  int x2 = x + topW,   y2 = y - h;
  int x3 = x + dx + botW, y3 = y;
  int x4 = x + dx,        y4 = y;

  lcd.Set_Draw_color(bottomColor);
  lcd.Fill_Triangle(x1,y1, x4,y4, x3,y3);
  lcd.Fill_Triangle(x1,y1, x2,y2, x3,y3);

  int th = h / 3;
  int midW = topW - ((topW - botW) * th) / h;
  int mdx  = (topW - midW) / 2;
  int yMid = y - h + th;
  int mx1  = x + mdx,        my1 = yMid;
  int mx2  = x + mdx + midW, my2 = yMid;
  lcd.Set_Draw_color(WHITE);
  lcd.Fill_Triangle(x1,y1, mx1,my1, x2,y2);
  lcd.Fill_Triangle(mx1,my1, mx2,my2, x2,y2);

  lcd.Set_Draw_color(BLACK);
  lcd.Draw_Line(x1,y1, x2,y2);
  lcd.Draw_Line(x2,y2, x3,y3);
  lcd.Draw_Line(x3,y3, x4,y4);
  lcd.Draw_Line(x4,y4, x1,y1);
}

// Fire a dotted red projectile
void fireProjectile() {
  const float maxSpeed = 50.0;
  const float g = maxSpeed*maxSpeed / float(SCREEN_W);
  float v0 = power/100.0 * maxSpeed;
  float rad = angle * PI/180.0;
  float vx = v0 * cos(rad), vy = v0 * sin(rad);
  float t = 0, dt = 0.2;
  int x0 = spawnXs[0] + tankWidth/2;
  int y0 = groundYs[0] - tankHeight - 2;
  while (true) {
    float xf = x0 + vx*t;
    float yf = y0 - (vy*t - 0.5*g*t*t);
    int xi = int(xf), yi = int(yf);
    if (xi < 0 || xi >= SCREEN_W || yi >= SCREEN_H) break;
    lcd.Set_Draw_color(RED);
    lcd.Fill_Rectangle(xi-2, yi-2, xi+2, yi+2);
    t += dt;
    delay(30);
  }
}

void setup() {
  Serial.begin(115200);

  // choose background randomly
  bool night = random(0,2) == 0;
  backgroundColor = night ? BLACK : WHITE;
  lcd.Init_LCD();
  lcd.Set_Rotation(1);
  lcd.Fill_Screen(backgroundColor);

  // configure buttons
  pinMode(ANGLE_DEC_PIN, INPUT_PULLUP);
  pinMode(ANGLE_INC_PIN, INPUT_PULLUP);
  pinMode(POWER_DEC_PIN, INPUT_PULLUP);
  pinMode(POWER_INC_PIN, INPUT_PULLUP);
  pinMode(SHOOT_PIN,     INPUT_PULLUP);

  // draw UI boxes in BLACK
  lcd.Set_Draw_color(BLACK);
  // Player 1 box
  lcd.Draw_Rectangle(
    p1x - uiBoxPad,
    healthY - uiBoxPad,
    p1x + barW + uiBoxPad + 30,  // extra for icon
    powerY + barH + uiBoxPad
  );
  // Player 2 box
  lcd.Draw_Rectangle(
    p2x - uiBoxPad - 30,
    healthY - uiBoxPad,
    p2x + barW + uiBoxPad,
    powerY + barH + uiBoxPad
  );

  // outline health & power bars
  lcd.Set_Draw_color(BLACK);
  lcd.Draw_Rectangle(p1x, healthY,  p1x+barW, healthY+barH);
  lcd.Draw_Rectangle(p1x, powerY,   p1x+barW, powerY +barH);
  lcd.Draw_Rectangle(p2x, healthY,  p2x+barW, healthY+barH);
  lcd.Draw_Rectangle(p2x, powerY,   p2x+barW, powerY +barH);

  // fill health (blue)
  lcd.Set_Draw_color(BLUE);
  lcd.Fill_Rectangle(p1x+1, healthY+1,  p1x+barW-1, healthY+barH-1);
  lcd.Fill_Rectangle(p2x+1, healthY+1,  p2x+barW-1, healthY+barH-1);

  // initial power fill (grey)
  lcd.Set_Draw_color(DARK_GREY);
  int w0 = map(power, 0, 100, 0, barW-2);
  lcd.Fill_Rectangle(p1x+1, powerY+1, p1x+1+w0, powerY+barH-1);
  lcd.Fill_Rectangle(p2x+1, powerY+1, p2x+1+w0, powerY+barH-1);

  // player names
  lcd.Set_Text_colour(BLACK);
  lcd.Set_Text_Back_colour(backgroundColor);
  lcd.Set_Text_Size(1);
  lcd.Print_String("Player 1", p1x, healthY - 12);
  lcd.Print_String("Player 2", p2x, healthY - 12);

  // draw tank icons
  drawTankIcon(p1x + barW + uiBoxPad + 10, healthY + barH + 8, GREEN);
  drawTankIcon(p2x - uiBoxPad - 30,         healthY + barH + 8, BLUE);

  // terrain & main tanks
  spawnXs[0] = SCREEN_W/3 + random(-20,20);
  spawnXs[1] = 2*SCREEN_W/3 + random(-20,20);
  generateTerrain();
  for (int i = 0; i < numTanks; i++) {
    groundYs[i] = max(groundYs[i], tankHeight + 1);
    drawTank(
      spawnXs[i],
      groundYs[i],
      WHITE,
      (i == 0 ? GREEN : BLUE)
    );
  }
}

void loop() {
  bool ad = digitalRead(ANGLE_DEC_PIN),
       ai = digitalRead(ANGLE_INC_PIN),
       pd = digitalRead(POWER_DEC_PIN),
       pi = digitalRead(POWER_INC_PIN),
       sh = digitalRead(SHOOT_PIN);

  bool needRedrawBarrel = false, needRedrawPower = false;

  // angle adjust, clamp 0–180
  if (ad==LOW && lastAdec==HIGH) {
    angle = max(0, angle - angleStep);
    needRedrawBarrel = true;
  }
  if (ai==LOW && lastAinc==HIGH) {
    angle = min(180, angle + angleStep);
    needRedrawBarrel = true;
  }

  // power adjust
  if (pd==LOW && lastPdec==HIGH) {
    power = max(0, power - powerStep);
    needRedrawPower = true;
  }
  if (pi==LOW && lastPinc==HIGH) {
    power = min(100, power + powerStep);
    needRedrawPower = true;
  }

  // shoot
  if (sh==LOW && lastShoot==HIGH) {
    fireProjectile();
  }

  // redraw barrel: erase old, draw new
  if (needRedrawBarrel) {
    int bx  = spawnXs[0] + tankWidth/2;
    int by  = groundYs[0] - tankHeight - 2;
    float prad = prevAngle * PI/180.0;
    int pex = bx + int(cos(prad)*16);
    int pey = by - int(sin(prad)*16);
    lcd.Set_Draw_color(backgroundColor);
    lcd.Draw_Line(bx, by, pex, pey);

    float rad = angle * PI/180.0;
    int ex = bx + int(cos(rad)*16);
    int ey = by - int(sin(rad)*16);
    lcd.Set_Draw_color(DARK_GREY);
    lcd.Draw_Line(bx, by, ex, ey);

    Serial.print("Angle = ");
    Serial.println(angle);
    prevAngle = angle;
  }

  // redraw power bar
  if (needRedrawPower) {
    int w = map(power, 0, 100, 0, barW-2);
    lcd.Set_Draw_color(backgroundColor);
    lcd.Fill_Rectangle(p1x+1, powerY+1, p1x+barW-1, powerY+barH-1);
    lcd.Set_Draw_color(DARK_GREY);
    lcd.Fill_Rectangle(p1x+1, powerY+1, p1x+1+w, powerY+barH-1);

    Serial.print("Power = ");
    Serial.println(power);
  }

  lastAdec   = ad;
  lastAinc   = ai;
  lastPdec   = pd;
  lastPinc   = pi;
  lastShoot  = sh;

  delay(30);
}
