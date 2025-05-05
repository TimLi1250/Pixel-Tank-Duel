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
#define BROWN       0xA145
#define DARK_GREY   0x8410
#define DARK_GREEN  0x03A0

// —— Tank settings ——
const int numTanks = 2;
int spawnXs[numTanks], groundYs[numTanks];
const int tankWidth  = 24;
const int tankHeight = 10;

// —— UI metrics ——
const int barW    = 100;
const int barH    =   8;
const int margin  =  10;
const int p1x     = margin;
const int healthY = margin;
const int powerY  = healthY + barH + 4;

// —— Button pins ——
#define ANGLE_DEC_PIN 2
#define ANGLE_INC_PIN 3
#define POWER_DEC_PIN 4
#define POWER_INC_PIN 5
#define SHOOT_PIN     6

// —— State variables ——
int angle     = 0;    // degrees, –90…+90
int power     = 50;   // 0…100
bool lastAdec = HIGH, lastAinc = HIGH;
bool lastPdec = HIGH, lastPinc = HIGH;
bool lastShoot= HIGH;

// Generate terrain (unchanged)
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

// Draw tank body and barrel at current angle
void drawTank(int x, int y) {
  // body
  lcd.Set_Draw_color(DARK_GREEN);
  int wB=tankWidth, wT=14, th=tankHeight;
  int botLx=x, botLy=y, botRx=x+wB;
  int topLy=y-th;
  int topLx=x+(wB-wT)/2, topRx=topLx+wT;
  lcd.Fill_Triangle(botLx,botLy, topLx,topLy, botRx,botLy);
  lcd.Fill_Triangle(topLx,topLy, topRx,topLy, botRx,botLy);
  // barrel
  float rad = angle * PI/180.0;
  int bx = x + wB/2;
  int by = y - th - 2;
  int len = 16;
  int ex = bx + int(cos(rad)*len);
  int ey = by - int(sin(rad)*len);
  lcd.Set_Draw_color(DARK_GREY);
  lcd.Draw_Line(bx, by, ex, ey);
  // outline
  lcd.Set_Draw_color(WHITE);
  lcd.Draw_Line(botLx,botLy, botRx,botLy);
  lcd.Draw_Line(botRx,botLy, topRx,topLy);
  lcd.Draw_Line(topRx,topLy, topLx,topLy);
  lcd.Draw_Line(topLx,topLy, botLx,botLy);
}

// Fire a dotted projectile from tank0
void fireProjectile() {
  // calibrate so full-power horizontal range ≈ SCREEN_W
  const float maxSpeed = 50.0;
  const float g = maxSpeed*maxSpeed / float(SCREEN_W);
  float v0 = power/100.0 * maxSpeed;
  float rad = angle * PI/180.0;
  float vx = v0 * cos(rad), vy = v0 * sin(rad);
  float t = 0, dt = 0.1;
  int x0 = spawnXs[0] + tankWidth/2;
  int y0 = groundYs[0] - tankHeight - 2;
  lcd.Set_Draw_color(BLUE);
  while (true) {
    float xf = x0 + vx*t;
    float yf = y0 - (vy*t - 0.5*g*t*t);
    int xi = int(xf), yi = int(yf);
    if (xi<0 || xi>=SCREEN_W || yi>=SCREEN_H) break;
    lcd.Fill_Rectangle(xi, yi, xi, yi);
    t += dt;
    delay(30);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ANGLE_DEC_PIN, INPUT_PULLUP);
  pinMode(ANGLE_INC_PIN, INPUT_PULLUP);
  pinMode(POWER_DEC_PIN, INPUT_PULLUP);
  pinMode(POWER_INC_PIN, INPUT_PULLUP);
  pinMode(SHOOT_PIN,     INPUT_PULLUP);

  lcd.Init_LCD();
  lcd.Set_Rotation(1);
  lcd.Fill_Screen(WHITE);

  // draw UI frames
  lcd.Set_Draw_color(WHITE);
  lcd.Draw_Rectangle(p1x, healthY, p1x+barW, healthY+barH);
  lcd.Draw_Rectangle(p1x, powerY,  p1x+barW, powerY +barH);
  int p2x = SCREEN_W - margin - barW;
  lcd.Draw_Rectangle(p2x, healthY, p2x+barW, healthY+barH);
  lcd.Draw_Rectangle(p2x, powerY,  p2x+barW, powerY +barH);

  // fill health
  lcd.Set_Draw_color(BLUE);
  lcd.Fill_Rectangle(p1x+1, healthY+1, p1x+barW-1, healthY+barH-1);

  // initial power bar
  int w0 = map(power, 0, 100, 0, barW-2);
  lcd.Set_Draw_color(DARK_GREY);
  lcd.Fill_Rectangle(p1x+1, powerY+1, p1x+1+w0, powerY+barH-1);

  // names
  lcd.Set_Text_colour(BLACK);
  lcd.Set_Text_Back_colour(WHITE);
  lcd.Set_Text_Size(1);
  lcd.Print_String("Player 1", p1x, healthY-10);
  lcd.Print_String("Player 2", p2x, healthY-10);

  // terrain & tanks
  spawnXs[0] = SCREEN_W/3 + random(-20,20);
  spawnXs[1] = 2*SCREEN_W/3 + random(-20,20);
  generateTerrain();
  for(int i=0;i<numTanks;i++){
    groundYs[i]=max(groundYs[i], tankHeight+1);
    drawTank(spawnXs[i], groundYs[i]);
  }
}

void loop() {
  // read and edge-detect buttons
  bool ad = digitalRead(ANGLE_DEC_PIN),
       ai = digitalRead(ANGLE_INC_PIN),
       pd = digitalRead(POWER_DEC_PIN),
       pi = digitalRead(POWER_INC_PIN),
       sh = digitalRead(SHOOT_PIN);
  bool redrawAngle=false, redrawPower=false;

  // angle adjust
  if (ad==LOW && lastAdec==HIGH) {
    angle = max(-90, angle - 5);
    redrawAngle = true;
  }
  if (ai==LOW && lastAinc==HIGH) {
    angle = min( 90, angle + 5);
    redrawAngle = true;
  }

  // power adjust
  if (pd==LOW && lastPdec==HIGH) {
    power = max(0, power - 5);
    redrawPower = true;
  }
  if (pi==LOW && lastPinc==HIGH) {
    power = min(100, power + 5);
    redrawPower = true;
  }

  // shoot
  if (sh==LOW && lastShoot==HIGH) {
    fireProjectile();
  }

  // redraw angle (barrel) if needed
  if (redrawAngle) {
    // Serial print
    Serial.print("Angle = ");
    Serial.println(angle);
    // redraw tank 1 barrel
    drawTank(spawnXs[0], groundYs[0]);
  }

  // redraw power bar if needed
  if (redrawPower) {
    // Serial print
    Serial.print("Power = ");
    Serial.println(power);
    int w = map(power, 0, 100, 0, barW-2);
    lcd.Set_Draw_color(WHITE);
    lcd.Fill_Rectangle(p1x+1, powerY+1, p1x+barW-1, powerY+barH-1);
    lcd.Set_Draw_color(DARK_GREY);
    lcd.Fill_Rectangle(p1x+1, powerY+1, p1x+1+w, powerY+barH-1);
  }

  // save last states
  lastAdec = ad;  lastAinc = ai;
  lastPdec = pd;  lastPinc = pi;
  lastShoot= sh;

  delay(30);
}
