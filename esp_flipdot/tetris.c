#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <stdbool.h> 

#include "tetris.h"
#include "pieces.h"
#include "fonttetris.h"


void refresh_screen() {
  for (int y = 0; y < BOARD_WIDTH; y++) {
    for (int x = 0; x < BOARD_HEIGHT; x++) {
      if (screen[y][x] > 0) dot_screen[y][BOARD_HEIGHT - 1 - x] = 1;
      else dot_screen[y][BOARD_HEIGHT - 1 - x] = 0;
    }
  }
  update_screen(dot_screen);
}

void convert_to_arr(char letter) {
  int ch;
  for (int col = 0; col < 5; col++) {
    ch = font57[letter - 32][col];
    for (int row = 0; row < BOARD_WIDTH; row++) {
      byte onebit = bitRead(ch, row);
      ch_out[row][col] = onebit;
    }
  }
}

void display_word(char str[6]) {
  int delta = 0;
  for (int num = 0; num < 5; num++) {
    //Serial.println(str[num]);
    convert_to_arr(str[num]);
    for (int row = 0; row < BOARD_WIDTH; row++) {
      for (int col = 0; col < 5; col++) {
        dot_screen[row][delta + col] = ch_out[row][col];
      }
    }
    delta = delta + 6;
  }
}


void update_screen(uint8_t new_screen[][BOARD_HEIGHT]) {
  for (int row = 0; row < BOARD_WIDTH; row++) {
    for (int col = 0; col < BOARD_HEIGHT; col++) {
      if (new_screen[row][col] != dot_screen_old[row][col]) {
        flipdot(row, 31 - col, new_screen[row][col]);
        dot_screen_old[row][col] = new_screen[row][col];

      }
    }
  }
}

void fill_screen(bool pattern) {
  for (int row = 0; row < BOARD_WIDTH; row++) {
    for (int col = 0; col < BOARD_HEIGHT; col++) {
      dot_screen[row][col] = pattern;
    }
  }
  update_screen(dot_screen);
}

int piece_GetBlockType (int pPiece, int pRotation, int pX, int pY) {
    return pPieces [pPiece][pRotation][pX][pY];
}


void board_InitBoard() {
  for (int y=0;y<BOARD_HEIGHT;y++) {
    for (int x=0;x<BOARD_WIDTH;x++) {
      board[x][y]=0;
    }}
}

void board_StorePiece(int pX, int pY, int pPiece, int pRotation) {
  for (int y=0;y<PIECE_BLOCKS;y++) {
    for (int x=0;x<PIECE_BLOCKS;x++) {
      if (piece_GetBlockType(pPiece,pRotation,x,y)!=0) 
        board[x+pX][y+pY]=3;
    }}
}

bool board_IsGameOver() {
  for (int x=0;x<BOARD_WIDTH; x++) {
    if (board[x][0]>0) return true;
  }
  return false;
}

void board_DeleteLine (int pY) {
  for (int y=pY;y>0;y--) {
    for (int x=0;x<BOARD_WIDTH;x++) {
      board[x][y]=board[x][y-1];
    }
  }
  deleted_lines++;
  deleted_line_num=pY;
  delay(80);
  deleted_line_num=0;
  delay(80);
  deleted_line_num=pY;
  delay(80);
  deleted_line_num=0;
}

void board_DeletePossibleLines () {
  for (int y=0;y<BOARD_HEIGHT;y++) {
    bool filled=true;
    int x=0;
    while (x<BOARD_WIDTH) {
      if (board[x][y]==0) {
        filled=false;
        break;
      } else x++;
    }
    if (filled) board_DeleteLine(y);
  }
}

bool board_IsFreeBlock (int pX, int pY)
{
    if (board[pX][pY] == 0) return true; else return false;
}

bool board_IsPossibleMovement (int pX, int pY, int pPiece, int pRotation) {
  for (int y=0;y<PIECE_BLOCKS;y++) {
    for (int x=0;x<PIECE_BLOCKS;x++) {
      // check board pieces
      if (piece_GetBlockType(pPiece,pRotation,x,y)!=0 && !board_IsFreeBlock(pX+x,pY+y)) return false;
      
      // check borders
      if (pX+x>BOARD_WIDTH-1 || pX+x<0|| pY+y>BOARD_HEIGHT-1) {
        if (piece_GetBlockType(pPiece,pRotation,x,y)!=0) return false;
      }
    }
  }
  return true;
}


int game_GetRand (int pA, int pB) {
  return rand () % (pB - pA + 1) + pA;
//  return random(pA,pB);
}

void game_InitGame() {
    // Init random numbers
   //srand ((unsigned int) time(NULL));
 
    // First piece
    mPiece          = game_GetRand (0, 6);
    mRotation       = game_GetRand (0, 3);
    mPosX           = (BOARD_WIDTH / 2) -2;
    mPosY           = 0;
 
    //  Next piece
    /* mNextPiece      = game_GetRand (0, 6);
    mNextRotation   = game_GetRand (0, 3);
    mNextPosX       = BOARD_WIDTH + 5;
    mNextPosY       = 5;     */

   deleted_lines=0;
   deleted_line_num=0;
}


void game_CreateNewPiece() {
    mPiece          = game_GetRand (0, 6);
    mRotation       = game_GetRand (0, 3);
    mPosX           = (BOARD_WIDTH / 2) -2;
    mPosY           = -1;
 
}


void game_DrawPiece (int pX, int pY, int pPiece, int pRotation) {
  for (int y=0;y<PIECE_BLOCKS;y++) {
    for (int x=0;x<PIECE_BLOCKS;x++) {
      if (piece_GetBlockType(pPiece,pRotation,x,y)!=0) screen[x+pX][y+pY]=piece_GetBlockType(pPiece,pRotation,x,y);
    }}
}

void game_DrawBoard () {
//  for (int y=0;y<BOARD_HEIGHT+2;y++) {
//  screen[0][y]=4;
//  screen[BOARD_WIDTH+1][y]=4;
//  }
//  for (int x=0;x<BOARD_WIDTH+2;x++) {
//    screen[x][BOARD_HEIGHT]=5;
//  }
  for (int y=0;y<BOARD_HEIGHT;y++) {
    for (int x=0;x<BOARD_WIDTH;x++) {
      screen[x][y]=board[x][y];
    }
  }
  
}

void clear_screen () {
  // init screen
  for (int y=0;y<BOARD_HEIGHT;y++) {
    for (int x=0;x<BOARD_WIDTH;x++) {
      screen[x][y]=0;
    }
  }
}
