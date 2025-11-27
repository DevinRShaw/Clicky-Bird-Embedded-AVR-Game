#include <avr/interrupt.h>
#include <avr/io.h>
#include "periph.h"
#include "helper.h"
#include "timerISR.h"
#include "serialATmega.h"
#include "serialATmega.h"
#include "spiAVR.h"
#include "EEPROM.h"
#include "LCD.h"
#include <time.h>

#define RED 0x001F
#define GREEN 0x07E0
#define WHITE 0xFFFF  
#define BLK 0x0000

#define BACKGROUND WHITE
#define PLAYER_COLOR BLK
#define PIPE_COLOR BLK

#define LEVEL_SIZE 128 //total unique frames to cycle through 
#define PIPE_SPACING 32 //space between each pipe, used in random generation of pipes 
#define PLAYER_OFFSET 31//screen x axis offset for player to be on screen at 
#define PLAYER_SIZE 10 
#define PIPE_WIDTH 16
#define GAP 32 



//CROSS-TASK COMMUNICATION 

  //SET BY TickButtons
  bool control; //USED BY TickMenu 
  bool jump; //USED BY TickPosition 

  //SET BY TickMenu
  enum GAME_STATE {PAUSE, PLAY, RESET};
  enum GAME_STATE game_state; //USED BY TickPosition, TickScreen/Game, TickBuzzer 

  //SET BY TickPosition
  int height = 64; //USED BY TickDeath

  //SET BY TickDeath
  bool dead = false; //USED BY TickMenu
  
  //SET BY TickLevel 
  struct column* curr_column; //USED BY TickDeath
  int score = 0; //SET TO 0 in TickDeath, USED BY TickScoreboard
  //track which frame we are on 
  int frame; 

  //SET BY TickScoreboard, SET TO score in TickDeath
  int high_score = EEPROM_read(EEPROM_SCORE_ADDR); //load in from EEPOM on intialization



//LEVEL CREATION LOGIC 

  struct column {
    //check collision, increment score, draw if there is a pipe 
    bool has_pipe = false;
    //standard gap size between top and bottom column, use for collision check
    const uint8_t gap = GAP; 
    int8_t bottom = -1; //top = bottom + gap
  };

  struct column columns[LEVEL_SIZE]; 

  void create_level(){
    int max = 86;
    int min = 10; 
    int range = max - min + 1;

    for (int i = 0; i < LEVEL_SIZE; i++){
      //if column is a pipe, randomly generate a bottom height, then fill the rest of columns within pipe_width
      if ( i % PIPE_SPACING == 0 && i != 0){
        columns[i].has_pipe = true;
        columns[i].bottom = rand() % range + min;
      }

      else if (i == 0){
        columns[i].has_pipe = false;
      }

      else {
        columns[i].has_pipe = false;
        columns[i].bottom = - 1;
      }
    }
  }

  //allows us to change a pipe's value off screen
  void refresh_pipe(int i){
    int max = 86;
    int min = 10; 
    int range = max - min + 1;

    columns[i - PLAYER_OFFSET].has_pipe = true;
    columns[i - PLAYER_OFFSET].bottom = rand() % range + min;
  }



//PERIPHIALS 
  void write_score(int write_score, int line){
    char buf[12];
    int len = 0;
    
    // Convert number to string
    itoa(write_score, buf, 10);
    
    // Calculate string length
    while(buf[len] != '\0') {
      len++;
    }
    
    // Position cursor at right-aligned position
    // Assuming 16 characters per line, start at (16 - length)
    lcd_goto_xy(line, 16 - len);
    
    // Write the entire string
    for (int i = 0; i < len; i++) {
      lcd_write_character(buf[i]);
    }
  }

  void scoreboard_init(){
    //read from EEPROM here for the highscore 
    //highscore = eeprom_read_word((uint16_t*)0);
    
    lcd_clear();
    lcd_goto_xy(0, 0);
    lcd_write_str("Score:");
    write_score(score, 0);
    
    lcd_goto_xy(1, 0);
    lcd_write_str("Best:");
    write_score(high_score, 1);
  }

  void draw_player() {

    static int last_height = -1; // initialize to an invalid value

    uint8_t x0 = PLAYER_OFFSET - PLAYER_SIZE/2;
    uint8_t x1 = PLAYER_OFFSET + PLAYER_SIZE/2;

    // Erase previous player position
    if (last_height >= PLAYER_SIZE/2) {
        uint8_t y0_old = last_height - PLAYER_SIZE/2;
        uint8_t y1_old = last_height + PLAYER_SIZE/2;
        SetWriteWindow(x0, y0_old, x1, y1_old);
        FillWindow(x0, y0_old, x1, y1_old, BACKGROUND); // erase with white
    }


    // Erase previous player position
    if (height >= PLAYER_SIZE/4) {

      // Draw new player position
      uint8_t y0_new = height - PLAYER_SIZE/4;
      uint8_t y1_new = height + PLAYER_SIZE/4;
      SetWriteWindow(x0, y0_new, x1, y1_new);
      FillWindow(x0, y0_new, x1, y1_new, PLAYER_COLOR); // draw with black
    }


    last_height = height;
  }

  void draw_pipe(column pipe, int x_pos) {

    uint8_t x0 = x_pos;
    uint8_t x1 = x_pos;
    uint8_t y0;
    uint8_t y1;

    // Wipe bottom pipe
    y0 = YS;
    y1 = pipe.bottom;
    SetWriteWindow(x0+1, y0, x1+1, y1);
    FillWindow(x0+1, y0, x1+1, y1, BACKGROUND); 

    // Wipe top pipe
    y0 = pipe.bottom + pipe.gap;
    y1 = YE;
    SetWriteWindow(x0+1, y0, x1+1, y1);
    FillWindow(x0+1, y0, x1+1, y1, BACKGROUND); 

    // Draw bottom pipe
    y0 = YS;
    y1 = pipe.bottom;
    SetWriteWindow(x0, y0, x1, y1);
    FillWindow(x0, y0, x1, y1, PIPE_COLOR); // draw with black

    // Draw top pipe
    y0 = pipe.bottom + pipe.gap;
    y1 = YE;
    SetWriteWindow(x0, y0, x1, y1);
    FillWindow(x0, y0, x1, y1, PIPE_COLOR); // draw with black
  }

  void draw_pipes() {
    // Only draw pipes that are in view
    for (int i = 0; i < LEVEL_SIZE; i++) {
      if (columns[i].has_pipe && i % PIPE_SPACING < PIPE_WIDTH - 1) {

        int screen_pos = i - frame + PLAYER_OFFSET;

        // Draw the current position
        if (screen_pos >= 0 && screen_pos < LEVEL_SIZE) {
          draw_pipe(columns[i], screen_pos);
        }
        
        // Draw the wrapped position (next revolution)
        int wrapped_pos = screen_pos + LEVEL_SIZE;
        if (wrapped_pos >= 0 && wrapped_pos < LEVEL_SIZE) {
          draw_pipe(columns[i], wrapped_pos);
        }
      }

    }
  }

  void FillBackground(int background = BACKGROUND) {
      // Set window to full screen using macros
      SetWriteWindow(XS, YS, XE, YE);
      // Fill the full window with white using macros
      FillWindow(XS, YS, XE, YE, background);
  }



//STATES AND TASKS 
enum DRAW_STATES{SETUP, DRAW};
int TickDraw(int state){
  switch(state){
    case(SETUP):
      SendCommand(REVERT);
      FillBackground();
      create_level();
      state = DRAW;
      break;

    case(DRAW):
      state = (game_state != RESET) ? DRAW : SETUP;
      break;
  }
  switch(state){
    case(SETUP):
      break;

    case(DRAW):

      if(game_state == PAUSE){
        SendCommand(INVERT);
        draw_player();
        draw_pipes();
      }
      else { 
        draw_player();
        draw_pipes();
        SendCommand(REVERT);
      }

      break;
  }
  return state;
}

enum BUTTON_STATE {IDLE, SET_CONTROL, SET_JUMP};
int TickButtons(int state){

  //transitions 
  switch(state){
    case(IDLE): 
      //transition + set flag for both buttons on push
      //left button is for control 
      if (GetBit(PINC, 0) && !GetBit(PINC,1)){
        control = true;
        state = SET_CONTROL;
      }
      //right button is for jump
      else if (GetBit(PINC, 1) && !GetBit(PINC, 0) ){
        jump = true;
        state = SET_JUMP;
      }

      break;

    case(SET_CONTROL):
      if (GetBit(PINC, 0)){
        state = SET_CONTROL;
      }
      else {
        state = IDLE;
      }

      break;

    case(SET_JUMP):
      if (GetBit(PINC, 1)){
        state = SET_JUMP;
        jump = false; //you only jump once per push, turn off on the first self loop so we jump one tick 
      }
      else {
        state = IDLE;
      }

      break;
  }

  //state actions  
  switch(state){
    case(IDLE): 
      control = false;
      jump = false; 
      break;

    case(SET_CONTROL):
      control = true; //redundant to show behavior 
      break;

    case(SET_JUMP):
      break;
  }

  return state;

}

enum MENU_STATE {PAUSED, HOLDING_PLAY_RESET, PLAYING, RESETTING, HOLDING_PAUSED};
int TickMenu(int state){

  static int cnt = 0; //control hold timer for reset 
  static int reset_timer = 30; // = ? this is how long we will hold in pause to reset the game 


  //transitions 
  switch(state){
    case(PAUSED):
      if (control){
        state = HOLDING_PLAY_RESET;
      }
      else {
        state = PAUSED;
      }
      break;

    case(HOLDING_PLAY_RESET):
      if (control && cnt < reset_timer){
        state = HOLDING_PLAY_RESET;

      }

      else if (!control && cnt < reset_timer){
        cnt = 0;
        state = PLAYING;
      }

      else if (cnt == reset_timer){
        cnt = 0;
        state = RESETTING;
      }
      break;

    case(PLAYING):
      if (!control){
        state = (dead) ? RESETTING : PLAYING;
      }

      else { 
        game_state = PAUSE;
        state = HOLDING_PAUSED;
      }
      break;

    case(RESETTING):
      state = HOLDING_PAUSED;
      game_state = PAUSE;
      break;

    case(HOLDING_PAUSED):
      if (control){
        state = HOLDING_PAUSED;
      }
      else {
        state = PAUSED;
      }
      break;
  }

  //state actions 
  switch(state){
    case(PAUSED):
      game_state = PAUSE;
      break;

    case(HOLDING_PLAY_RESET):
      cnt++;
      break;

    case(PLAYING):
      game_state = PLAY;
      break;

    case(RESETTING):
      game_state = PLAY;
      dead = false;
      game_state = RESET;
      curr_column = &columns[0];

      if (score > high_score){
        high_score = score - 1;
        write_score(high_score, 1);
        EEPROM_write_score(EEPROM_SCORE_ADDR, high_score);
      }
      score = 0;

      scoreboard_init();

      break;

    case(HOLDING_PAUSED):
      break;
  }


  return state;
}

enum POSITION_STATES {FALLING, JUMPING, FREEZE, RESTART};
int TickPosition(int state){

  //TODO TUNE THESE USING SERIAL MONITOR AND DISPLAY DIMENSIONS 
  static unsigned int cnt = 0;
  static unsigned int speed; //how much position is changing each tick of FALLING
  const unsigned int accel = 1; //acceleration downwards 
  const unsigned int vertical = 3; //how high we ascend each tick of JUMPING  
  const unsigned int hangtime = 5; //how many ticks to ascend by vertical  
  const unsigned int start_height = 64; //where we start each time a new game is played 
  const unsigned int start_speed = 0; //we start not moving then we accelerate downwards 
  
  //transitions 
  switch(state){
    case(FALLING):
      if (jump && game_state != RESET && game_state != PAUSE){
        state = JUMPING;
      }
      else if (game_state == RESET){
        height = start_height;
        speed = start_speed;
        state = RESTART; 
      }

      else if (game_state == PAUSE){
        state = FREEZE;
      }

      else {
        state = FALLING;
      }
      break;

    case(JUMPING):
      if (cnt < hangtime && game_state != RESET && game_state != PAUSE && !dead){
        state = JUMPING;
        cnt = jump ? 0 : cnt; //reset count if we press jump during a jump
      }

      else if (cnt == hangtime && game_state != RESET){
        speed = start_speed; 
        cnt = 0;
        state = FALLING;
      }

      else if(game_state == PAUSE){
        state = FREEZE;
      }

      else if (game_state == RESET){
        height = start_height;
        speed = start_speed;
        state = RESTART;
        cnt = 0;
      }
      break;

    case(FREEZE):
      if(game_state == PLAY){
        //if we were mid jump, then cnt > 0, then we should continue our jump 
        state = (cnt != 0) ? JUMPING : FALLING; 
      }

      else if (game_state == PAUSE){
        state = FREEZE;
      }

      else if (game_state == RESET){
        state = RESTART;
      }
      break;

    case(RESTART):
      height = start_height;
      speed = start_speed;
      state = FREEZE;
      break;
  }

  //state actions 
  switch(state){
    case(FALLING):
      speed += accel;
      height -= speed; 
      break;

    case(JUMPING):
      cnt++;      
      height += vertical;
      break;

    case(FREEZE):
      break;

    case(RESTART): 
      break;
  }


  return state;

}

enum LEVEL_STATES {STOP, GO};
int TickLevel(int state){
  static int i = 0;
  frame = i;
  //transitions and state actions together
  switch(state){
    case(STOP):
      //only state action is keeping i at 0
      if (game_state == PLAY){
        state = GO;
      }
      else {
        i = (game_state == RESET) ? 0 : i;
        state = STOP;
      }
      break;

    case(GO):
      if (game_state == PLAY){
        //change the heights of just passed pipe for the next revolution
        //moment a pipe is off screen, refresh its value 

        if (i % PIPE_SPACING == 1 && i != 0){
          write_score(score, 0);
          score++;
        }
        //i can be offset, but only if we draw pipes as they come
        if (i % PIPE_SPACING == PIPE_SPACING - 1 && i != 0 ){ 
          refresh_pipe(i);
        }

        i = (i < LEVEL_SIZE - 1) ? i + 1 : 0;

      }
      else {
        i = (game_state == RESET) ? 0 : i;
        state = STOP;
      }
      break;
  }

  curr_column = &columns[i];
  return state;
}

enum DEATH_STATES{CHECK};
int TickDeath (int state){
  //hit ground or ceiling
  if (height < 0 || 128 < height){
    dead = true; 
    height = 64;
  }

  //intersection with current column
  else if (curr_column->has_pipe && (height <  curr_column->bottom || (curr_column->bottom + curr_column->gap) < height)){
    dead = true; 
  }

  else {
    dead = false;
    for(int i = frame - PLAYER_SIZE/2; i < frame + PLAYER_SIZE/2 + 1; i++){
      int wrapped_i = (i < 0) ? i + LEVEL_SIZE : (i >= LEVEL_SIZE) ? i - LEVEL_SIZE: i;
      column check_column = columns[wrapped_i];
      if (check_column.has_pipe && (height - PLAYER_SIZE/4  + 1 <  check_column.bottom || (check_column.bottom + check_column.gap) < height + PLAYER_SIZE/4)){
        dead = true;
      }
    }
  }

  return state;
}


//TASK SCHEDULING 
  
  // Task struct for concurrent synchSMs implmentations
  typedef struct _task{
    unsigned int state; 		//Task's current state
    unsigned long period; 		//Task period
    unsigned long elapsedTime; 	//Time elapsed since last task tick
    int (*TickFct)(int); 		//Task tick function
  } task;

  //TASK_PERIODS
  const unsigned long TASK1_PERIOD = 100;
  const unsigned long TASK2_PERIOD = 100;

  const unsigned long GCD_PERIOD = findGCD(TASK1_PERIOD, TASK2_PERIOD);

  #define NUM_TASKS 6 /*number of task here*/
  task tasks[NUM_TASKS]; // declared task array with 5 tasks (2 in exercise 1)

  void TimerISR() {
      
    for ( unsigned int i = 0; i < NUM_TASKS; i++ ) {                   // Iterate through each task in the task array
      if ( tasks[i].elapsedTime == tasks[i].period ) {           // Check if the task is ready to tick
        tasks[i].state = tasks[i].TickFct(tasks[i].state); // Tick and set the next state for this task
        tasks[i].elapsedTime = 0;                          // Reset the elapsed time for the next tick
      }
      tasks[i].elapsedTime += GCD_PERIOD;                        // Increment the elapsed time by GCD_PERIOD
    }
  }


int main(void) {
  //TODO: initialize all your inputs and ouputs
  DDRC  = 0x00;
  PORTC = 0xFF;

  DDRB  = 0xFF;
  PORTB = 0x00 & ~( 1 << RESET_PIN);

  DDRD  = 0xFF;
  PORTD = 0x00;

  srand(time(NULL));
  SPI_INIT();
  ST7735_init();
  
  serial_init(9600);

  int j = 0;
  tasks[j].period = TASK1_PERIOD;
  tasks[j].state = IDLE;
  tasks[j].elapsedTime = 0;
  tasks[j].TickFct = &TickButtons;

  j++;
  tasks[j].period = TASK1_PERIOD;
  tasks[j].state = RESTART;
  tasks[j].elapsedTime = 0;
  tasks[j].TickFct = &TickPosition;

  j++;
  tasks[j].period = TASK1_PERIOD;
  tasks[j].state = CHECK;
  tasks[j].elapsedTime = 0;
  tasks[j].TickFct = &TickDeath;

  j++;
  tasks[j].period = TASK1_PERIOD;
  tasks[j].state = PAUSED;
  tasks[j].elapsedTime = 0;
  tasks[j].TickFct = &TickMenu;

  j++;
  tasks[j].period = TASK1_PERIOD;
  tasks[j].state = STOP;
  tasks[j].elapsedTime = 0;
  tasks[j].TickFct = &TickLevel;

  j++;
  tasks[j].period = TASK2_PERIOD;
  tasks[j].state = SETUP;
  tasks[j].elapsedTime = 0;
  tasks[j].TickFct = &TickDraw;

  create_level();
  lcd_init();
  _delay_ms(500);
  scoreboard_init();

  TimerSet(GCD_PERIOD);
  TimerOn();

  while(1){}

}

