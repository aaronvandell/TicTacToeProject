#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// Wi-Fi credentials
const char* ssid = "Aaron iPhone";
const char* password = "youareaperson1";

// IP of the machine running Mosquitto (on your local network)
const char* mqtt_server = "35.233.235.105";

WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);
int gameBoard[9] = {0};
int xWins = 0, oWins = 0, draws = 0, gamesPlayed = 0;

void setup_wifi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}


void callback(char* topic, byte* message, unsigned int length){
  String msg = "";
  for (int i = 0; i < length; i++){
    msg += (char)message[i];
  }
  int move = msg.toInt();

  if (String(topic == "game/reset"))
  {
    resetBoard();
    xWins = 0;
    oWins = 0;
    draws = 0;
    gamesPlayed = 0;
    updateLCD();
  }

  if (String(topic) == "player/x")
  {
    gameBoard[move] = 1;
  } else if (String(topic) == "player/o"){
    gameBoard[move] = 2;
  }

  int winner = checkWinner();
  if (winner == 1){
    xWins++;
    gamesPlayed++;
    updateLCD();
    resetBoard();
  } else if (winner == 2){
    oWins++;
    gamesPlayed++;
    updateLCD();
    resetBoard();
  } else if (isDraw()){
    draws++;
    gamesPlayed++;
    updateLCD();
    resetBoard();
  }
  client.publish("gameboard", gameBoardReturn());
}

void reconnect(){
  while (!client.connected()){
    if (client.connect("ESP32Client")){
      client.subscribe("player/x");
      client.subscribe("player/o");
    } else{
      delay(2000);
    }
  }
}

void setup() {
  Wire.begin(14, 13);
  lcd.init();
  lcd.backlight();
  updateLCD();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

int checkWinner(){
  int winCombos[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
    {0, 4, 8}, {2, 4, 6}
  };

  for (int i = 0; i < 8; i++){
    int a = winCombos[i][0];
    int b = winCombos[i][1];
    int c = winCombos[i][2];
    if (gameBoard[a] != 0 && gameBoard[a] == gameBoard[b] && gameBoard[b] == gameBoard[c]){
      return gameBoard[a];
    }
  }
  return 0;
}

char* gameBoardReturn()
{
  static char board[10];
  for (int i = 0; i < 9; i++)
  {
    char marker;
    if (gameBoard[i] == 1){
      marker = 'X';
    } else if (gameBoard[i] == 2) {
      marker = 'O';
    } else {
      marker = '-';
    }
    board[i] = marker;
  }
  board[9] = '\0';
  return board;
}

bool isDraw(){
  for (int i = 0; i < 9; i++){
    if (gameBoard[i] == 0) return false;
  }
  return true;
}

void resetBoard(){
  for (int i = 0; i < 9; i++){
    gameBoard[i] = 0;
  }
}


void updateLCD(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("X:");
  lcd.print(xWins);
  lcd.print(" O:");
  lcd.print(oWins);
  lcd.setCursor(0,1);
  lcd.print("Draws:");
  lcd.print(draws);
}

void loop() {
  if (!client.connected()){
    reconnect();
  }
  client.loop();
}

