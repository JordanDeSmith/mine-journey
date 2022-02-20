/*
 * Mine Journey
 * By Jordan Smith
 * 
*/
#define FLASH_LENGTH 2000
#define TIMER_LENGTH 2000

////data////
enum Data {SETUP = 17, SENDING, SENDING_TO_OTHER, RECEIVING, READY, CONNECTED, DISCONNECTED, GAME_OVER, VICTORY, WON, LOST, EMPTY, MINE, END, START, PLAYING};
byte state = SETUP;
Timer endTimer;

////Colors////


////Parents and children for map propogation////
const byte NO_PARENT = 6;
const byte MASTER = 7;
byte parentFace = NO_PARENT;
byte sendingFace = 0;
bool isSending = false;

////Map////
byte north = 0;
byte location[2];
const byte MAP_HEIGHT = 16;
const byte MAP_WIDTH = 16;
byte mineMap[MAP_HEIGHT][MAP_WIDTH] = {
    {EMPTY, MINE, EMPTY, MINE, EMPTY, MINE, EMPTY, EMPTY, EMPTY, EMPTY, MINE, EMPTY, EMPTY, MINE, EMPTY, EMPTY},
    {EMPTY, EMPTY, MINE, EMPTY, EMPTY, MINE, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, MINE, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, MINE, EMPTY, EMPTY, MINE, EMPTY, MINE, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, MINE, EMPTY, EMPTY, EMPTY, MINE, EMPTY, MINE, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, MINE, EMPTY, EMPTY, MINE, EMPTY, MINE, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, MINE, MINE, EMPTY, MINE, MINE, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, MINE, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, MINE, EMPTY, MINE, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, MINE, EMPTY, MINE, EMPTY, EMPTY, EMPTY, EMPTY, MINE, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, MINE, EMPTY, EMPTY, EMPTY, EMPTY, MINE, EMPTY, EMPTY, MINE, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, START, EMPTY, EMPTY, MINE, EMPTY, EMPTY, EMPTY, EMPTY}
  };
byte currentRow = 0;

void setup() {
  randomize(); //Seed random generator
}

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

  buttonDoubleClicked(); //Resets double click flag, just in case
}

/////////////////////
///////Setup/////////
/////////////////////

void createMap() {
  //Randomly decide where the start and end are
  //Randomly place out mines in other spaces
  //Need to determine where current blink is, and make sure no others are where a bomb would be...  
}

void sendingLoop() {
  if (!isSending) {
    for (byte f = sendingFace; f < FACE_COUNT; f++) {
      if (!isValueReceivedOnFaceExpired(f) && (getLastValueReceivedOnFace(f) == SETUP || 
        getLastValueReceivedOnFace(f) == GAME_OVER || getLastValueReceivedOnFace(f) == VICTORY)) {
        sendingFace = f;
        isSending = true;
        setValueSentOnFace(SENDING, f);
        break;
      }
    }
  }

  if (isSending) {
    if (currentRow < MAP_HEIGHT) {
      if (!isValueReceivedOnFaceExpired(sendingFace) && getLastValueReceivedOnFace(sendingFace) == currentRow) {
        sendDatagramOnFace(&mineMap[currentRow++], MAP_WIDTH, sendingFace);
      }
      setColor(dim(GREEN, map(currentRow, 0, MAP_HEIGHT, 0, 255)));
      flashColorOnFace(BLUE, sendingFace);
    }
    else {
      if (!isValueReceivedOnFaceExpired(sendingFace) && getLastValueReceivedOnFace(sendingFace) == READY) {
        currentRow = 0;
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
  setValueSentOnFace(currentRow, parentFace);
  if (isDatagramReadyOnFace(parentFace)) {
    byte *datagram = getDatagramOnFace(parentFace);
    byte dataLength = getDatagramLengthOnFace(parentFace);
    if (dataLength == MAP_WIDTH) {
      markDatagramReadOnFace(parentFace);
      for (byte i=0; i < dataLength; i++) {
        mineMap[currentRow][i] = *(datagram + i);
      }
      ++currentRow;
      setValueSentOnFace(currentRow, parentFace);
    }
    else {
      setColor(ORANGE);
      return;
    }
  }
  if (currentRow >= MAP_HEIGHT) {
    state = SENDING;
    currentRow = 0;
  }
  setColor(dim(RED, map(currentRow, 0, MAP_HEIGHT, 0, 255)));
  flashColorOnFace(BLUE, parentFace);
}

void readyLoop() {
  //If master, tell all to begin game by sending location data
  if (parentFace == MASTER) {
    for (byte y = 0; y < MAP_HEIGHT; y++) {
        for (byte x = 0; x < MAP_WIDTH; x++) {
          if (mineMap[y][x] == START) {
            location[0] = x;
            location[1] = y;
            setValueSentOnAllFaces(PLAYING);
            sendLocationData();
            state = CONNECTED;
            setColor(OFF);
            setColorOnFace(BLUE, north);
            return;
          }
        }
      }
  }
  //Otherwise listen for start signal
  if (getLocation()) {
    setColor(WHITE);
    state = DISCONNECTED;
    setValueSentOnAllFaces(PLAYING);
    sendLocationData();
  }

  flashColorOnFace(GREEN, 6);
}

void standbyLoop() {
  //Wait for starting press
  if (buttonDoubleClicked()) {
    //createMap();
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
      setValueSentOnAllFaces(currentRow);
      return;
    }
  }

  //Set lights
  flashColorOnFace(BLUE, 6);
}


////////////////////
///////Game/////////
////////////////////

void disconnectedLoop() {
  if (buttonLongPressed()) {
    gameEnd(false);
  }
  
  //Check if we have a location signal
  flashColorOnFace(CYAN, 6);
  if (getLocation()) {
    //Found a face transmitting a location!
    state = CONNECTED;
    if (!((0 <= location[0] && location[0] < MAP_WIDTH) && (0 <= location[1] && location[1] < MAP_HEIGHT))) {
      setColor(OFF);
      return;
    }
    else {setColor(dim(WHITE, 255/4));}
    Color flashColor = YELLOW;
    byte space = mineMap[location[1]][location[0]];
    if (space == EMPTY) {
      byte mineCount = 0;
      if (location[1] + 1 < MAP_HEIGHT && mineMap[location[1] + 1][location[0]] == MINE) {
        ++mineCount;
      }
      if (location[1] - 1 >= 0 && mineMap[location[1] - 1][location[0]] == MINE) {
        ++mineCount;
      }
      if (location[0] - 1 >= 0 && mineMap[location[1]][location[0] - 1] == MINE) {
        ++mineCount;
      }
      if (location[0] + 1 < MAP_HEIGHT && mineMap[location[1]][location[0] + 1] == MINE) {
        ++mineCount;
      }
      int rowModifier = -1;
      if ((location[0] % 2) == 0) { //Even column
        rowModifier = 1;
      }
      if (rowModifier == -1) {
      }
      if ((location[0] - 1 >= 0) && (0 <= (location[1] + rowModifier)) && ((location[1] + rowModifier) < MAP_HEIGHT) && (mineMap[location[1] + rowModifier][location[0] - 1] == MINE)) {
        ++mineCount;
      }
      if ((location[0] + 1 < MAP_WIDTH) && (0 <= (location[1] + rowModifier)) && ((location[1] + rowModifier) < MAP_HEIGHT) && (mineMap[location[1] + rowModifier][location[0] + 1] == MINE)) {
        ++mineCount;
      }
      for (byte i = 0; i < mineCount; i++) {
        setColorOnFace(flashColor, i);
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
  //TODO: Make an indication that reset is about to happen
  if (buttonLongPressed()) {
    gameEnd(false);
  }
  
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
  sendLocationData();
}

void gameEndLoop(bool won) {
  //Reset data
  currentRow = 0;
  sendingFace = 0;
  isSending = false;
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
  FOREACH_FACE(f) {
    byte newX = location[0];
    byte newY = location[1];
      //Sends location, and north data
      byte faceCorrectedForNorth = (f + FACE_COUNT - north) % 6;
      switch (faceCorrectedForNorth) {
        case 0:
          --newY;
          break;
        case 1:
          ++newX;
          if (!evenColumn) {
            --newY;
          }
          break;
        case 2:
          ++newX;
          if (evenColumn) {
            ++newY;
          }
          break;
        case 3:
          ++newY;
          break;
        case 4:
          --newX;
          if (evenColumn) {
            ++newY;
          }
          break;
        case 5:
          --newX;
          if (!evenColumn) {
            --newY;
          }
          break;
      }
      byte data[] = {faceCorrectedForNorth , newX, newY};
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
  endTimer.set(TIMER_LENGTH);
}
