#include <LCDWIKI_GUI.h>
#include <LCDWIKI_SPI.h>
#include <math.h>
#include <SPI.h>

// Display Setup
#define MODEL       ILI9488_18
#define CS_PIN      10
#define CD_PIN       9
#define RST_PIN      8
#define LED_PIN     -1

LCDWIKI_SPI lcd(MODEL, CS_PIN, CD_PIN, RST_PIN, LED_PIN);

// Screen Dimensions
#define SCREEN_W 480
#define SCREEN_H 320

// Colors 
#define BLACK       0x0000
#define WHITE       0xFFFF
#define BLUE        0x001F
#define GREEN       0x03A0
#define RED         0xF800
#define BROWN       0xA145
#define DARK_GREY   0x8410
#define GOLD        0xF8A0

// Tank and UI Settings
const int numTanks   = 2;
int spawnXs[numTanks], groundYs[numTanks];
const int tankWidth  = 16;
const int tankHeight = 6;
const int barrelLen  = 10; 

// UI layout
const int barW      = 100;
const int barH      =   8;
const int margin    =  10;
const int topOffset =  15;
const int p1x       = margin;
const int p2x       = SCREEN_W - margin - barW;
const int healthY   = margin + topOffset;
const int powerY    = healthY + barH + 4;
const int uiBoxPad  =   5;


const int maxHealth = 100;
int healthArr[2] = { maxHealth, maxHealth };

// Button Pins
#define ANGLE_DEC_PIN 2
#define ANGLE_INC_PIN 3
#define POWER_DEC_PIN 4
#define POWER_INC_PIN 5
#define SHOOT_PIN     6

// State Variables
int currentPlayer = 0;
int angleArr[2]     = { 90, 90 };
int prevAngleArr[2] = { 90, 90 };
int powerArr[2]     = { 50, 50 };

const int angleStep = 5;
const int powerStep = 2; 
bool lastAdec   = HIGH, lastAinc = HIGH;
bool lastPdec   = HIGH, lastPinc = HIGH;
bool lastShoot  = HIGH;

// background color
uint16_t backgroundColor;

// terrain sampling
const int chunkW = 5;
const int nChunks = SCREEN_W / chunkW;
static uint16_t chunkHeights[nChunks];

// Drawing trajectory
int trajX[30];
int trajY[30];
int trajLen = 0;

unsigned long lastBlinkTime = 0;
bool blinkOn = true;
const unsigned long blinkInterval = 500;


// Generate terrain
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
      chunkHeights[x/chunkW] = hi;
    }
    h = target;
  }
}

// Draw a trapezoid-shaped tank 
void drawTank(int x, int y, uint16_t topColor, uint16_t bottomColor) {
  int topW = tankWidth;
  int botW = tankWidth * 2 / 3;
  int h    = tankHeight;
  int dx   = (topW - botW) / 2;

  int x1 = x,           y1 = y - h;  // top-left
  int x2 = x + topW,    y2 = y - h;  // top-right
  int x3 = x + dx + botW, y3 = y;    // bot-right
  int x4 = x + dx,        y4 = y;    // bot-left

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

  // barrel
  float rad = angleArr[0] * PI/180.0;
  int bx = x + topW/2;
  int by = y - h - 2;
  int ex = bx + int(cos(rad)*barrelLen);
  int ey = by - int(sin(rad)*barrelLen);
  lcd.Set_Draw_color(DARK_GREY);
  lcd.Draw_Line(bx, by, ex, ey);

  // outline in BLACK
  lcd.Set_Draw_color(BLACK);
  lcd.Draw_Line(x1,y1, x2,y2);
  lcd.Draw_Line(x2,y2, x3,y3);
  lcd.Draw_Line(x3,y3, x4,y4);
  lcd.Draw_Line(x4,y4, x1,y1);
}

// Draw small tank icon next to UI
void drawTankIcon(int x, int y, uint16_t bottomColor) {
  const int w = 20, h = 10;
  int topW = w;
  int botW = w * 2 / 3;
  int dx   = (topW - botW) / 2;

  int x1 = x,           y1 = y - h;
  int x2 = x + topW,    y2 = y - h;
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
// Draw Health Bars
void drawHealthBars() {
  // erase old
  lcd.Set_Draw_color(backgroundColor);
  lcd.Fill_Rectangle(p1x+1, healthY+1, p1x+barW-1, healthY+barH-1);
  lcd.Fill_Rectangle(p2x+1, healthY+1, p2x+barW-1, healthY+barH-1);

  // draw new
  lcd.Set_Draw_color(BLUE);
  int w0 = map(healthArr[0], 0, maxHealth, 0, barW-2);
  int w1 = map(healthArr[1], 0, maxHealth, 0, barW-2);
  lcd.Fill_Rectangle(p1x+1, healthY+1, p1x+1+w0, healthY+barH-1);
  lcd.Fill_Rectangle(p2x+1, healthY+1, p2x+1+w1, healthY+barH-1);
}

// Fire Projectile
void fireProjectile(int player) {
  trajLen        = 0;
  int lastChunk  = -1;
  float prevY    = 0.0f;

  int uiBottom = powerY + barH + uiBoxPad;

  const float maxSpeed = 60.0;
  const float g        = maxSpeed*maxSpeed / float(SCREEN_W);
  float v0   = powerArr[player]/100.0 * maxSpeed;
  float rad  = angleArr[player] * PI/180.0;
  float vx   = v0 * cos(rad), vy = v0 * sin(rad);
  float t    = 0, dt = 0.4;

  int x0 = spawnXs[player] + tankWidth/2;
  int y0 = groundYs[player] - tankHeight - 2;

  while (true) {
    // projectile position
    float xf = x0 + vx*t;
    float yf = y0 - (vy*t - 0.5*g*t*t);
    int   xi = int(xf), yi = int(yf);

    // stop if off screen
    if (xi < 0 || xi >= SCREEN_W || yi >= SCREEN_H) break;

    // only sample once per terrain chunk
    int chunkIdx = xi / chunkW;
    if (chunkIdx != lastChunk && chunkIdx >= 0 && chunkIdx < nChunks) {
      // terrain collision
      int terrainH = chunkHeights[chunkIdx];
      if (lastChunk >= 0) {
        int prevTerrain = chunkHeights[lastChunk];
        if ((yi - terrainH) * (prevY - prevTerrain) <= 0) {
          // explode some terrain
          lcd.Set_Draw_color(backgroundColor);
          lcd.Fill_Rectangle(xi-5, terrainH-5, xi+5, terrainH+5);
          break;
        }
      }

      prevY     = yi;
      lastChunk = chunkIdx;

      // tank collision
      int opp = 1 - player;
      int margin = chunkW;
      int tx0 = spawnXs[opp] - margin;
      int tx1 = spawnXs[opp] + tankWidth + margin;
      int ty0 = groundYs[opp] - tankHeight - margin;
      int ty1 = groundYs[opp] + margin;
      if (xi >= tx0 && xi <= tx1 && yi >= ty0 && yi <= ty1) {
        int dmg = maxHealth/3 + 1;
        healthArr[opp] = max(0, healthArr[opp] - dmg);
        drawHealthBars();
        break;
      }
   
      if (yi > uiBottom) {
        lcd.Set_Draw_color(RED);
        lcd.Fill_Rectangle(xi-1, yi-1, xi+1, yi+1);

        if (trajLen < 30) {
          trajX[trajLen] = xi;
          trajY[trajLen] = yi;
          trajLen++;
        }
      }
    }
    // increment t
    t += dt;
    delay(30);
  }
}

// Erase Trajectory
void eraseTrajectory() {

  for (int i = 0; i < trajLen; i++) {
    int x = trajX[i];
    int y = trajY[i];

    uint16_t terrainH = chunkHeights[x/chunkW];

    if (y < terrainH) {
      lcd.Set_Draw_color(WHITE);
    } else {
      lcd.Set_Draw_color(BROWN);
    }
    lcd.Fill_Rectangle(x-1, y-1, x+1, y+1);
  }
  for (int i = 0; i < trajLen; i++) {
    trajX[i] = 0;
    trajY[i] = 0;
  }
  trajLen = 0;
}

// Displays Title
void showStartScreen() {

  uint16_t textColor = BLACK;

  lcd.Set_Text_colour(textColor);
  lcd.Set_Text_Back_colour(backgroundColor);


  lcd.Set_Text_Size(3);
  lcd.Print_String("Pixel Tank", 150, SCREEN_H/3);

  lcd.Set_Text_Size(2);
  lcd.Print_String("Press any button to begin!", 96, SCREEN_H*2/3);

  while ( digitalRead(ANGLE_DEC_PIN)==HIGH &&
          digitalRead(ANGLE_INC_PIN)==HIGH &&
          digitalRead(POWER_DEC_PIN)==HIGH &&
          digitalRead(POWER_INC_PIN)==HIGH &&
          digitalRead(SHOOT_PIN)==HIGH ) {
    delay(10);
  }
}
// Displays Game Over screen
void showGameOver(int winner) {
  uint16_t txtCol = BLACK;

  lcd.Fill_Screen(backgroundColor);

  lcd.Set_Text_colour(txtCol);
  lcd.Set_Text_Back_colour(backgroundColor);
  lcd.Set_Text_Size(3);
  lcd.Print_String(F("GAME OVER"), 159, SCREEN_H/3);

  char buf[16];
  sprintf(buf, "Player %d won!", winner + 1);
  lcd.Set_Text_Size(2);
  int msgX = (SCREEN_W - 6 * strlen(buf) * 2) / 2;
  int msgY = SCREEN_H/2;
  lcd.Print_String(buf, msgX, msgY);

  uint16_t tankColor = (winner == 0 ? GREEN : BLUE);
  int iconX = SCREEN_W/2-10;
  int iconY = msgY + 50;
  drawTankIcon(iconX, iconY, tankColor);

  while (true) {
    delay(100);
  }
}
// Setup
void setup() {
  Serial.begin(115200);
  SPI.begin();

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  backgroundColor = WHITE;
  lcd.Init_LCD();
  lcd.Set_Rotation(1);

  // configure buttons
  pinMode(ANGLE_DEC_PIN, INPUT_PULLUP);
  pinMode(ANGLE_INC_PIN, INPUT_PULLUP);
  pinMode(POWER_DEC_PIN, INPUT_PULLUP);
  pinMode(POWER_INC_PIN, INPUT_PULLUP);
  pinMode(SHOOT_PIN,     INPUT_PULLUP);

  lcd.Fill_Screen(backgroundColor);
  showStartScreen();
  //lcd.Fill_Screen(backgroundColor);
  lcd.Fill_Screen(backgroundColor);


  lcd.Set_Draw_color(BLACK);
  // Player 1 box
  lcd.Draw_Rectangle(
    p1x - uiBoxPad,
    healthY - uiBoxPad - 14,
    p1x + barW + uiBoxPad + 40,
    powerY + barH + uiBoxPad
  );
  // Player 2 box
  lcd.Draw_Rectangle(
    p2x - uiBoxPad - 40,
    healthY - uiBoxPad - 14,
    p2x + barW + uiBoxPad,
    powerY + barH + uiBoxPad
  );

  // outline health and power bars
  lcd.Set_Draw_color(BLACK);
  lcd.Draw_Rectangle(p1x, healthY,  p1x+barW, healthY+barH);
  lcd.Draw_Rectangle(p1x, powerY,   p1x+barW, powerY +barH);
  lcd.Draw_Rectangle(p2x, healthY,  p2x+barW, healthY+barH);
  lcd.Draw_Rectangle(p2x, powerY,   p2x+barW, powerY +barH);

  // fill health
  lcd.Set_Draw_color(BLUE);
  lcd.Fill_Rectangle(p1x+1, healthY+1,  p1x+barW-1, healthY+barH-1);
  lcd.Fill_Rectangle(p2x+1, healthY+1,  p2x+barW-1, healthY+barH-1);

  drawHealthBars();

  // initial power
  lcd.Set_Draw_color(DARK_GREY);
  int w0 = map(powerArr[0], 0, 100, 0, barW-2);
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

  // terrain and main tanks
  // IMPORTANT check later
  spawnXs[0] = SCREEN_W/5 + random(-4, 5) * 5;
  spawnXs[1] = 4*SCREEN_W/5 + random(-4, 5) * 5;
  generateTerrain();

  for (int i = 0; i < numTanks; i++) {
    groundYs[i] = max(groundYs[i]+2, tankHeight + 1);
    drawTank(
      spawnXs[i],
      groundYs[i],
      WHITE,
      (i == 0 ? GREEN : BLUE)
    );
  }
}
// Loop
void loop() {
  bool ad = digitalRead(ANGLE_DEC_PIN),
       ai = digitalRead(ANGLE_INC_PIN),
       pd = digitalRead(POWER_DEC_PIN),
       pi = digitalRead(POWER_INC_PIN),
       sh = digitalRead(SHOOT_PIN);

  bool needRedrawBarrel = false, needRedrawPower = false;
  int  p = currentPlayer;

  // adjust angle
  if (ad==LOW && lastAdec==HIGH) {
    angleArr[p] = max(0, angleArr[p] - angleStep);
    needRedrawBarrel = true;
  }
  if (ai==LOW && lastAinc==HIGH) {
    angleArr[p] = min(180, angleArr[p] + angleStep);
    needRedrawBarrel = true;
  }

  // adjust power
  if (pd==LOW && lastPdec==HIGH) {
    powerArr[p] = max(0, powerArr[p] - powerStep);
    needRedrawPower = true;
  }
  if (pi==LOW && lastPinc==HIGH) {
    powerArr[p] = min(100, powerArr[p] + powerStep);
    needRedrawPower = true;
  }

  // redraw barrel
  if (needRedrawBarrel) {
    int bx = spawnXs[p] + tankWidth/2;
    int by = groundYs[p] - tankHeight - 2;

    // erase old
    float prad = prevAngleArr[p] * PI/180.0;
    lcd.Set_Draw_color(backgroundColor);
    lcd.Draw_Line(bx, by,
                  bx + int(cos(prad)*barrelLen),
                  by - int(sin(prad)*barrelLen));

    // draw new
    float rad = angleArr[p] * PI/180.0;
    lcd.Set_Draw_color(DARK_GREY);
    lcd.Draw_Line(bx, by,
                  bx + int(cos(rad)*barrelLen),
                  by - int(sin(rad)*barrelLen));

    Serial.print("P"); Serial.print(p+1);
    Serial.print(" angle="); Serial.println(angleArr[p]);
    prevAngleArr[p] = angleArr[p];
  }

  // redraw the power bar for current player
  if (needRedrawPower) {
    int baseX = (p==0 ? p1x : p2x);
    int w = map(powerArr[p], 0, 100, 0, barW-2);
    lcd.Set_Draw_color(backgroundColor);
    lcd.Fill_Rectangle(baseX+1, powerY+1,
                       baseX+barW-1, powerY+barH-1);
    lcd.Set_Draw_color(DARK_GREY);
    lcd.Fill_Rectangle(baseX+1, powerY+1,
                       baseX+1+w, powerY+barH-1);

    Serial.print("P"); Serial.print(p+1);
    Serial.print(" power="); Serial.println(powerArr[p]);
  }

  // shoot and then switch the player turn
  if (sh==LOW && lastShoot==HIGH) {
    fireProjectile(p);
    eraseTrajectory();

    int opp = 1 - p;
    if (healthArr[opp] <= 0) {
      showGameOver(p);
    }

    currentPlayer = opp;
  }

  unsigned long now = millis();
  if (now - lastBlinkTime >= blinkInterval) {
    lastBlinkTime = now;
    blinkOn = !blinkOn;

    int nameY = healthY - 12;
    int nameH = 8;

    // string widths
    int p1NameW = 6 * strlen("Player 1");
    int p2NameW = 6 * strlen("Player 2");

    lcd.Set_Text_Size(1);
    lcd.Set_Text_colour(GOLD);
    lcd.Set_Text_Back_colour(backgroundColor);
    if (currentPlayer == 0) {
      lcd.Print_String(F("Player 2"), p2x, nameY);
    } else {
      lcd.Print_String(F("Player 1"), p1x, nameY);
    }

    if (currentPlayer == 0) {
      if (blinkOn) {
        lcd.Print_String(F("Player 1"), p1x, nameY);
      } else {
        lcd.Set_Draw_color(backgroundColor);
        lcd.Fill_Rectangle(p1x, nameY,
                           p1x + p1NameW,
                           nameY + nameH);
      }
    } else {
      if (blinkOn) {
        lcd.Print_String(F("Player 2"), p2x, nameY);
      } else {
        lcd.Set_Draw_color(backgroundColor);
        lcd.Fill_Rectangle(p2x, nameY,
                           p2x + p2NameW,
                           nameY + nameH);
      }
    }

    lcd.Set_Draw_color(BLACK);
  }

  lastAdec   = ad;
  lastAinc   = ai;
  lastPdec   = pd;
  lastPinc   = pi;
  lastShoot  = sh;

  delay(30);
}
