/*
 * Mine Journey
 * By Jordan Smith
 * 
*/
////Constants////
#define FLASH_LENGTH 8
#define END_TIMER_LENGTH 2000
#define SPIN_TIMER_LENGTH 1000
#define NO_PARENT 6
#define MASTER 7
#define NORTH_COLOR GREEN
#define READY_GREEN makeColorRGB(0, 140, 0)
#define EMPTY_COLOR dim(WHITE, 255/4)
#define MINE_INDICATOR_COLOR YELLOW
#define MAP_HEIGHT 20
#define MAP_WIDTH 12
#define MINE_MAX 45

////data////
enum Data {SETUP = 50, RESOLVING, SENDING, SENDING_TO_OTHER, RECEIVING, READY, CONNECTED, DISCONNECTED, GAME_OVER, VICTORY};
byte state = SETUP;
byte subState = 60;
static Timer endTimer;

////Data for map propogation////
byte parentFace = NO_PARENT;
byte sendingFace = 0;
bool isSending = false;

////Map////
byte north = 0;
byte mineCount = 0;
byte indicatorFace = 0;
static Timer spinTimer;
bool isEdge = false;
bool isMarker = false;
char location[2];
char mineMap[MINE_MAX][2];
byte difficulty = 1;
byte totalMines = 20;



////Setup function////
void setup() {
  setValueSentOnAllFaces(SETUP);
  randomize(); //Seed random generator
}

////Main loop////
void loop() {
  switch(state) {
    case SETUP:
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

  //Listen for end signal, unless we're already sending end
  if (subState >= 0) {
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
  }

  if (isMarker && spinTimer.isExpired()) {
    spinTimer.set(SPIN_TIMER_LENGTH);
    setColorOnFace(OFF, indicatorFace);
    indicatorFace = (indicatorFace + 1) % 6;
    setColorOnFace(RED, indicatorFace);
  }
  else if (mineCount > 0 && spinTimer.isExpired()) {
    spinTimer.set(SPIN_TIMER_LENGTH);
    for (byte i = 0; i < mineCount; i++) {
      setColorOnFace(MINE_INDICATOR_COLOR, (indicatorFace + i) % FACE_COUNT);
    }
    for (byte i = mineCount; i < FACE_COUNT; i++) {
      setColorOnFace(EMPTY_COLOR, (indicatorFace + i) % FACE_COUNT);
    }
    indicatorFace = (indicatorFace + 1) % 6;
  }
  
  buttonDoubleClicked(); //Resets double click flag, just in case
  buttonPressed();
}




/////////////////////
///////Setup/////////
/////////////////////

void createMap() {
  //Randomly place out mines in other spaces
  setValueSentOnAllFaces(state);
  for (byte i = 0; i < totalMines; i++) {
    bool duplicate = false;
    byte randX;
    byte randY;
    do {
      duplicate = false;
      randX = random(MAP_WIDTH - 1);
      randY = random(MAP_HEIGHT - 1);
      for (byte j = 0; j < i; j++) {
        if (mineMap[j][0] == randX && mineMap[j][1] == randY) {
            duplicate = true;
            break;
        }
      }
    } while (duplicate || (randX >= MAP_WIDTH / 2 - 1 && randX < MAP_WIDTH / 2 + 2 && randY <= 1));
    mineMap[i][0] = randX;
    mineMap[i][1] = randY;
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
    if (subState < totalMines) {
      if (!isValueReceivedOnFaceExpired(sendingFace) && getLastValueReceivedOnFace(sendingFace) == subState) {
        sendDatagramOnFace(&mineMap[subState], 2, sendingFace);
        ++subState;
      }
    }
    else {
      if (!isValueReceivedOnFaceExpired(sendingFace) && getLastValueReceivedOnFace(sendingFace) == READY) {
        subState = 0;
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
  }
  setColor(dim(READY_GREEN, map(subState, 0, totalMines, 0, 255)));
  flashColorOnFace(BLUE, sendingFace);
}

void receivingLoop() {
  setValueSentOnFace(subState, parentFace);
  if (isDatagramReadyOnFace(parentFace)) {
    byte *datagram = getDatagramOnFace(parentFace);
    byte dataLength = getDatagramLengthOnFace(parentFace);
    if (dataLength == 2) {
      markDatagramReadOnFace(parentFace);
      for (byte i=0; i < dataLength; i++) {
        mineMap[subState][i] = *(datagram + i);
      }
      ++subState;
      setValueSentOnFace(subState, parentFace);
    }
    else {
      setColor(ORANGE);
      return;
    }
  }
  if (subState >= totalMines) {
    state = SENDING;
    subState = 0;
  }
  setColor(dim(BLUE, map(subState, 0, totalMines, 0, 255)));
  flashColorOnFace(BLUE, parentFace);
}

void readyLoop() {
  //If master, tell all to begin game by sending location data
  if (parentFace == MASTER) {
    location[0] = MAP_WIDTH/2;
    location[1] = 0;
    state = CONNECTED;
    setColor(EMPTY_COLOR);
    sendLocationData();
    return;
  }
  //Otherwise listen for start signal
  if (getLocation()) {
    state = DISCONNECTED;
    sendLocationData();
  }

  flashColorOnFace(READY_GREEN, 6);
}

void standbyLoop() {
  //Wait for starting press
  bool canStart = false;
  if (!isAlone()) {   //Don't want to start if it's alone, or it won't share the map with anyone
    byte connectionCount = 0;
    byte connectionFace = 6;
    FOREACH_FACE(f) {
      if (isValueReceivedOnFaceExpired(f)) {
        if (isValueReceivedOnFaceExpired((f+1)%6) && isValueReceivedOnFaceExpired((f+2)%6)) {
          canStart = true;
          north = (f + 1) % 6;
        }
      }
      else {
        ++connectionCount;
        connectionFace = f;
      }
    }
    if (connectionCount == 1) {
      north = (connectionFace + 9) % 6;
    }
    else if (connectionCount == 2) {
      canStart = false;
    }
  }
  if (canStart) {
    setColor(WHITE);
    indicatorFace = (north + 2) % 6;
    setColorOnFace(NORTH_COLOR, north);
  
    if (buttonPressed()) {
      createMap();
      state = SENDING;
      parentFace = MASTER;
      setValueSentOnAllFaces(SENDING_TO_OTHER);
      setColor(OFF);
      subState = 0;
      return;
    }
  }
  else {
    flashColorOnFace(BLUE, 6);
  }

  if (buttonDoubleClicked()) {
    if (++difficulty > 3) {
      difficulty = 1;
    }
    subState = 61;
    setValueSentOnAllFaces(difficulty);
  }

  if (subState == 60) {
    setValueSentOnAllFaces(state);
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        if (getLastValueReceivedOnFace(f) >= 1 && getLastValueReceivedOnFace(f) <= 3) {
          difficulty = getLastValueReceivedOnFace(f);
          setValueSentOnAllFaces(difficulty);
          subState = 61;
        }
      }
    }
  }
  if (subState == 61) {
    bool neighborsReady = true;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f) && getLastValueReceivedOnFace(f) == SETUP) {
        neighborsReady = false;
        break;
      }
    }
    if (neighborsReady) {
      setValueSentOnAllFaces(RESOLVING);
      subState = 62;
    }
  }
  else if (subState == 62) {
    bool neighborsReady = true;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f) && getLastValueReceivedOnFace(f) != SETUP && getLastValueReceivedOnFace(f) != RESOLVING) {
        neighborsReady = false;
        break;
      }
    }
    if (neighborsReady) {
        subState = 60;
        setValueSentOnAllFaces(SETUP);
    }
  }
  
  switch(difficulty) {
    case 1:
      totalMines = 20;
      flashColorOnFace(ORANGE, indicatorFace);
      break;
    case 2:
      totalMines = 32;
      flashColorOnFace(ORANGE, indicatorFace);
      flashColorOnFace(ORANGE, (indicatorFace + 1) % FACE_COUNT);
      break;
    case 3:
      totalMines = MINE_MAX;
      flashColorOnFace(ORANGE, indicatorFace);
      flashColorOnFace(ORANGE, (indicatorFace + 1) % FACE_COUNT);
      flashColorOnFace(ORANGE, (indicatorFace + 2) % FACE_COUNT);
      break;
  }
  
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && getLastValueReceivedOnFace(f) == SENDING) {
      parentFace = f;
      state = RECEIVING;
      setValueSentOnAllFaces(state);
      subState = 0;
      return;
    }
  }
}


////////////////////
///////Game/////////
////////////////////

void disconnectedLoop() {
  mineCount = 0;
  isEdge = false;
  location[0] = -1;
  location[1] = -1;

  if (buttonDoubleClicked()) {
    isMarker = !isMarker;
    setColor(OFF);
  }
  
  if (!isMarker) {
    flashColorOnFace(CYAN, 6);
  }

  //Check if we have a location signal
  if (getLocation()) {
    //Found a face transmitting a location!
    state = CONNECTED;
    byte space = 0;
    if (isMarker) {
      if (location[0] < 0 || location[0] >= MAP_WIDTH || location[1] < 0) {
        isEdge = true;
      }
      return;
    }
    if (location[1] > MAP_HEIGHT) {
      space = 1;
    }
    else if (location[0] < 0 || location[0] >= MAP_WIDTH || location[1] < 0) {
      setColor(OFF);
      setColorOnFace(NORTH_COLOR, north);
      isEdge = true;
      return;
    }
    else {
      for (byte i = 0; i < totalMines; ++i) {
        if (mineMap[i][0] == location[0] && mineMap[i][1] == location[1]) {
          space = 2;
        }
      }
    }
    if (space == 0) {
      setColor(EMPTY_COLOR);
      char rowModifier = 1;
      if ((location[0] % 2) == 0) { //Even column
        rowModifier = -1;
      }
      for (byte i = 0; i < totalMines; ++i) {
        if (location[0] == mineMap[i][0] && (location[1] - 1) == mineMap[i][1]) {
          ++mineCount;
        }
        else if (location[0] == mineMap[i][0] && (location[1] + 1) == mineMap[i][1]) {
          ++mineCount;
        }
        else if ((location[0] - 1) == mineMap[i][0] && location[1] == mineMap[i][1]) {
          ++mineCount;
        }
        else if ((location[0] + 1) == mineMap[i][0] && location[1] == mineMap[i][1]) {
          ++mineCount;
        }
        else if ((location[0] - 1) == mineMap[i][0] && (location[1] + rowModifier) == mineMap[i][1]) {
          ++mineCount;
        }
        else if ((location[0] + 1) == mineMap[i][0] && (location[1] + rowModifier) == mineMap[i][1]) {
          ++mineCount;
        }
      }
    }
    else if (space == 1) {
      gameEnd(true);
    }
    else if (space == 2) {
      gameEnd(false);
    }
  }
}

void connectedLoop() {
  if (buttonPressed()) {
    setColorOnFace(NORTH_COLOR, north);
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
  if (subState == -1) {
    bool neighborsReady = true;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f) && getLastValueReceivedOnFace(f) != VICTORY 
          && getLastValueReceivedOnFace(f) != GAME_OVER && getLastValueReceivedOnFace(f) != RESOLVING) {
        neighborsReady = false;
        break;
      }
    }
    if (neighborsReady) {
      setValueSentOnAllFaces(RESOLVING);
      subState = -2;
    }
  }
  else if (subState == -2 && endTimer.isExpired()) {
    bool neighborsReady = true;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f) && getLastValueReceivedOnFace(f) != SETUP 
          && getLastValueReceivedOnFace(f) != RESOLVING) {
        neighborsReady = false;
        break;
      }
    }
    if (neighborsReady) {
      subState = 60;
      FOREACH_FACE(f) {
        markDatagramReadOnFace(f);
      }
      state = SETUP;
      setValueSentOnAllFaces(state);
    }
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
  byte dimness = map(sin8_C((millis() / FLASH_LENGTH)), 0, 255, 127, 255);
  if (f >= 6) {
    setColor(dim(color, dimness));
  }
  else {
    setColorOnFace(dim(color, dimness), f);
  }
}

void gameEnd(bool won) {
  //Reset data
  location[0] = location[1] = -1;
  sendingFace = 0;
  mineCount = 0;
  indicatorFace = 0;
  isSending = false;
  isMarker = false;
  isEdge = false;
  parentFace = NO_PARENT;
  subState = -1;
  
  if (won) {
    state = VICTORY;
  }
  else {
    state = GAME_OVER;
  }
  setValueSentOnAllFaces(state);
  endTimer.set(END_TIMER_LENGTH);
}
