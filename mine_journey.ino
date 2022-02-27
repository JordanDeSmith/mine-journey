/*
 * Mine Journey
 * By Jordan Smith
 * 
*/
////Constants////
#define FLASH_LENGTH 2000
#define END_TIMER_LENGTH 2000
#define SPIN_TIMER_LENGTH 1000
#define NO_PARENT 6
#define MASTER 7
#define NORTH_COLOR GREEN
#define READY_GREEN makeColorRGB(0, 140, 0)
#define EMPTY_COLOR dim(WHITE, 255/4)
#define MINE_INDICATOR_COLOR YELLOW
#define MAP_HEIGHT 20
#define MAP_WIDTH 10
#define MINE_MAX 45

////data////
enum Data {SETUP = 50, SENDING, SENDING_TO_OTHER, RECEIVING, READY, CONNECTED, DISCONNECTED, GAME_OVER, VICTORY, WON, LOST, EMPTY, MINE, END, START, PLAYING};
static byte state = SETUP;
static Timer endTimer;

////Data for map propogation////
static byte parentFace = NO_PARENT;
static byte sendingFace = 0;
static bool isSending = false;
static byte currentMine = 0;

////Map////
static byte north = 0;
static byte mineCount = 0;
static byte mineIndicatorFace = 0;
static Timer spinTimer;
bool isEdge = false;
static char location[2];
static byte mineMap[MINE_MAX][2];





////Setup function////
void setup() {
  randomize(); //Seed random generator
}

////Main loop////
void loop() {
  switch(state) {
    case SETUP:
      setValueSentOnAllFaces(SETUP);
      standbyLoop();
      break;
    case RECEIVING:
      receivingLoop();
      break;
    case SENDING:
      sendingLoop();
      break;
    case READY:
      readyLoop();
      break;
    case CONNECTED:
      connectedLoop();
      break;
    case DISCONNECTED:
      disconnectedLoop();
      break;
    case GAME_OVER:
      gameEndLoop(false);
      break;
    case VICTORY:
      gameEndLoop(true);
      break;
  }

  if (buttonMultiClicked()) {
    gameEnd(false);
  }

  if (mineCount > 0 && spinTimer.isExpired()) {
    spinTimer.set(SPIN_TIMER_LENGTH);
    setColor(EMPTY_COLOR);
    for (byte i = 0; i < mineCount; i++) {
      setColorOnFace(MINE_INDICATOR_COLOR, (mineIndicatorFace + i) % FACE_COUNT);
    }
    mineIndicatorFace = (mineIndicatorFace + 1) % 6;
  }
  
  buttonDoubleClicked(); //Resets double click flag, just in case
}




/////////////////////
///////Setup/////////
/////////////////////

void createMap() {
  //Randomly place out mines in other spaces
  bool resetting;
  for (byte i = 0; i < MINE_MAX; i++) {
    resetting = false;
    mineMap[i][0] = random(MAP_WIDTH - 1);
    mineMap[i][1] = random(MAP_HEIGHT - 1);
    for (byte x = (MAP_WIDTH / 2) - 1; x < (MAP_WIDTH / 2) + 2; x++) {
      for (byte y = 0; y < 2; y++) {
        if (mineMap[i][0] == x && mineMap[i][1] == y) {
          --i;
          resetting = true;
        }
      }
    }
    for (byte j = 0; j < i; j++) {
      if (!resetting && mineMap[j][0] == mineMap[i][0] && mineMap[j][1] == mineMap[i][1]) {
        --i;
      }
    }
  }
}

void sendingLoop() {
  if (!isSending) {
    for (byte f = sendingFace; f < FACE_COUNT; f++) {
      if (!isValueReceivedOnFaceExpired(f) && getLastValueReceivedOnFace(f) == SETUP) {
        sendingFace = f;
        isSending = true;
        setValueSentOnFace(SENDING, f);
        break;
      }
    }
  }

  if (isSending) {
    if (currentMine < MINE_MAX) {
      if (!isValueReceivedOnFaceExpired(sendingFace) && getLastValueReceivedOnFace(sendingFace) == currentMine) {
        sendDatagramOnFace(&mineMap[currentMine], 2, sendingFace);
        ++currentMine;
      }
      setColor(dim(GREEN, map(currentMine, 0, MINE_MAX, 0, 255)));
      flashColorOnFace(BLUE, sendingFace);
    }
    else {
      if (!isValueReceivedOnFaceExpired(sendingFace) && getLastValueReceivedOnFace(sendingFace) == READY) {
        currentMine = 0;
        isSending = false;
      }
    }
  }
  else {
    //All children done
    FOREACH_FACE(f) {
      markDatagramReadOnFace(f);
    }
    state = READY;
    setValueSentOnAllFaces(state);
    flashColorOnFace(GREEN, 6);
  }
}

void receivingLoop() {
  setValueSentOnFace(currentMine, parentFace);
  if (isDatagramReadyOnFace(parentFace)) {
    byte *datagram = getDatagramOnFace(parentFace);
    byte dataLength = getDatagramLengthOnFace(parentFace);
    if (dataLength == 2) {
      markDatagramReadOnFace(parentFace);
      for (byte i=0; i < dataLength; i++) {
        mineMap[currentMine][i] = *(datagram + i);
      }
      ++currentMine;
      setValueSentOnFace(currentMine, parentFace);
    }
    else {
      setColor(ORANGE);
      return;
    }
  }
  if (currentMine >= MINE_MAX) {
    state = SENDING;
    currentMine = 0;
  }
  setColor(dim(RED, map(currentMine, 0, MINE_MAX, 0, 255)));
  flashColorOnFace(BLUE, parentFace);
}

void readyLoop() {
  //If master, tell all to begin game by sending location data
  if (parentFace == MASTER) {
    location[0] = MAP_WIDTH/2;
    location[1] = 0;
    setValueSentOnAllFaces(PLAYING);
    state = CONNECTED;
    setColor(EMPTY_COLOR);
    setColorOnFace(BLUE, north);
    return;
  }
  //Otherwise listen for start signal
  if (getLocation()) {
    setColor(WHITE);
    state = DISCONNECTED;
    setValueSentOnAllFaces(PLAYING);
    sendLocationData();
  }

  flashColorOnFace(READY_GREEN, 6);
}

void standbyLoop() {
  //TODO: Only start if this blink has 3 consecutive open sides. Make north the middle of the three. 
  //Wait for starting press
  if (buttonDoubleClicked()) {
    createMap();
    state = SENDING;
    parentFace = MASTER;
    setValueSentOnAllFaces(SENDING_TO_OTHER);
    setColor(OFF);
    return;
  }
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && getLastValueReceivedOnFace(f) == SENDING) {
      parentFace = f;
      state = RECEIVING;
      setValueSentOnAllFaces(currentMine);
      return;
    }
  }

  //Set lights
  flashColorOnFace(BLUE, 6);
  setColorOnFace(GREEN, north); //For debugging
}


////////////////////
///////Game/////////
////////////////////

void disconnectedLoop() {
  //Check if we have a location signal
  mineCount = 0;
  isEdge = false;
  flashColorOnFace(CYAN, 6);
  location[0] = -1;
  location[1] = -1;
  
  if (getLocation()) {
    //Found a face transmitting a location!
    state = CONNECTED;
    byte space = EMPTY;
    if (location[1] > MAP_HEIGHT) {
      space = END;
    }
    else if (location[0] < 0 || location[0] >= MAP_WIDTH || location[1] < 0) {
      setColor(OFF);
      setColorOnFace(NORTH_COLOR, north);
      isEdge = true;
      return;
    }
    else {
      for (byte i = 0; i < MINE_MAX; ++i) {
        if (mineMap[i][0] == location[0] && mineMap[i][1] == location[1]) {
          space = MINE;
        }
      }
    }
    if (space == EMPTY) {
      setColor(EMPTY_COLOR);
      char rowModifier = 1;
      if ((location[0] % 2) == 0) { //Even column
        rowModifier = -1;
      }
      for (byte i = 0; i < MINE_MAX; ++i) {
        if (location[0] == mineMap[i][0] && (location[1] - 1) == mineMap[i][1]) {//location[1] - 1 >= 0 && mineMap[location[1] - 1][location[0]] == MINE) {
          ++mineCount;
        }
        else if (location[0] == mineMap[i][0] && (location[1] + 1) == mineMap[i][1]) {//location[1] + 1 < MAP_HEIGHT && mineMap[location[1] + 1][location[0]] == MINE) {
          ++mineCount;
        }
        else if (location[0] == (mineMap[i][0] - 1) && location[1] == mineMap[i][1]) {//location[0] - 1 >= 0 && mineMap[location[1]][location[0] - 1] == MINE) {
          ++mineCount;
        }
        else if (location[0] == (mineMap[i][0] + 1) && location[1] == mineMap[i][1]) {//location[0] + 1 < MAP_HEIGHT && mineMap[location[1]][location[0] + 1] == MINE) {
          ++mineCount;
        }
        else if (location[0] == (mineMap[i][0] - 1) && (location[1] + rowModifier) == mineMap[i][1]) {//(location[0] - 1 >= 0) && (0 <= (location[1] + rowModifier)) && ((location[1] + rowModifier) < MAP_HEIGHT) && (mineMap[location[1] + rowModifier][location[0] - 1] == MINE)) {
          ++mineCount;
        }
        else if (location[0] == (mineMap[i][0] + 1) && (location[1] + rowModifier) == mineMap[i][1]) {//(location[0] + 1 < MAP_WIDTH) && (0 <= (location[1] + rowModifier)) && ((location[1] + rowModifier) < MAP_HEIGHT) && (mineMap[location[1] + rowModifier][location[0] + 1] == MINE)) {
          ++mineCount;
        }
      }
    }
    else if (space == END) {
      gameEnd(true);
    }
    else if (space == MINE) {
      gameEnd(false);
    }
  }
}

void connectedLoop() {
  if (buttonPressed()) {
    setColorOnFace(NORTH_COLOR, north);
  }
  /*if (buttonReleased()) {
    Color revertColor = EMPTY_COLOR;
    if (isNorthYellow) {
      revertColor = YELLOW;
    }
    setColorOnFace(revertColor, north);
  }*/
  
  //Listen for end signal
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      if (getLastValueReceivedOnFace(f) == GAME_OVER) {
        gameEnd(false);
      }
      else if (getLastValueReceivedOnFace(f) == VICTORY) {
        gameEnd(true);
      }
    }
  }
  //Check if we've been disconnected (alone)
  if (isAlone()) {
    FOREACH_FACE(f) {
      markDatagramReadOnFace(f);
    }
    state = DISCONNECTED;
  }
  if (!isEdge) {
    sendLocationData();
  }
}

void gameEndLoop(bool won) {
  //Reset data
  location[0] = location[1] = 0;
  currentMine = 0;
  sendingFace = 0;
  mineCount = 0;
  mineIndicatorFace = 0;
  isSending = false;
  parentFace = NO_PARENT;
  FOREACH_FACE(f) {
    markDatagramReadOnFace(f);
  }

  
  //Check if timer has expired
  if (endTimer.isExpired()) {
    setColor(BLUE);
    state = SETUP;
  }

  //Flash lights
  Color color = RED;
  if(won) {
    color = GREEN;
  }
  flashColorOnFace(color, 6);
}


/////////////////////////
////Helper Functions////
///////////////////////
void sendLocationData() {
  //Since hexes don't make perfect grids, assume a row "bends" up and down as it goes along
  //They "bend" up when moving from an even to an odd row, (counting 0 as even)
  //and down when moving from an odd to an even row.
  bool evenColumn = location[0]%2 == 0;
  byte data[3];
  byte newX;
  byte newY;
  FOREACH_FACE(f) {
    newX = location[0];
    newY = location[1];
    //Sends location, and north data
    byte faceCorrectedForNorth = (f + FACE_COUNT - north) % 6;
    switch (faceCorrectedForNorth) {
      case 0:
        ++newY;
        break;
      case 1:
        ++newX;
        if (!evenColumn) {
          ++newY;
        }
        break;
      case 2:
        ++newX;
        if (evenColumn) {
          --newY;
        }
        break;
      case 3:
        --newY;
        break;
      case 4:
        --newX;
        if (evenColumn) {
          --newY;
        }
        break;
      case 5:
        --newX;
        if (!evenColumn) {
          ++newY;
        }
        break;
    }
    data[0] = faceCorrectedForNorth;
    data[1] = newX;
    data[2] = newY;
    sendDatagramOnFace(&data, sizeof(data), f);
  }
}

//Returns boolean if location data was properly retrieved or not. 
bool getLocation() {
  FOREACH_FACE(f) {
    if (isDatagramReadyOnFace(f) && getDatagramLengthOnFace(f) == 3) {
      byte *data = getDatagramOnFace(f);
      north = (f + 9 - *data) % 6;
      location[0] = *(data+1);
      location[1] = *(data+2);
      return true;
    }
  }
  return false;
}

void flashColorOnFace(Color color, byte f) {
  byte dimness = sin8_C(map((millis() % FLASH_LENGTH), 0, FLASH_LENGTH, 0, 255));
  if (f >= 6) {
    setColor(dim(color, dimness));
  }
  else {
    setColorOnFace(dim(color, dimness), f);
  }
}

void gameEnd(bool won) {
  if (won) {
    state = VICTORY;
  }
  else {
    state = GAME_OVER;
  }
  setValueSentOnAllFaces(state);
  endTimer.set(END_TIMER_LENGTH);
}
