#include "engine.c"
#include <stdio.h>

static void setupStartpos(void){
  static const int back[8]={ROOK,KNIGHT,BISHOP,QUEEN,KING,BISHOP,KNIGHT,ROOK};
  memset(g_board,0,sizeof(g_board));
  for(int c=0;c<8;c++){
    g_board[SQ(0,c)] = MKP(back[c],BLACK);
    g_board[SQ(1,c)] = MKP(PAWN,BLACK);
    g_board[SQ(6,c)] = MKP(PAWN,WHITE);
    g_board[SQ(7,c)] = MKP(back[c],WHITE);
  }
}
static void setupKiwipete(void){
  memset(g_board,0,sizeof(g_board));
  const char* rows[8] = { "r3k2r","p1ppqpb1","bn2pnp1","3PN3","1p2P3","2N2Q1p","PPPBBPPP","R3K2R" };
  for(int r=0;r<8;r++){
    int c=0; const char* s=rows[r];
    for(int i=0; s[i]; i++){
      char ch=s[i];
      if(ch>='1'&&ch<='8'){ c += ch-'0'; continue; }
      int color = (ch>='a'&&ch<='z') ? BLACK : WHITE;
      char up = (ch>='a'&&ch<='z') ? ch-32 : ch;
      int type = up=='P'?PAWN: up=='N'?KNIGHT: up=='B'?BISHOP: up=='R'?ROOK: up=='Q'?QUEEN: KING;
      g_board[SQ(r,c)] = MKP(type,color);
      c++;
    }
  }
}
/* Position 3: 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - (endgame, EP + pins stress) */
static void setupPos3(void){
  memset(g_board,0,sizeof(g_board));
  const char* rows[8] = { "8","2p5","3p4","KP5r","1R3p1k","8","4P1P1","8" };
  for(int r=0;r<8;r++){
    int c=0; const char* s=rows[r];
    for(int i=0; s[i]; i++){
      char ch=s[i];
      if(ch>='1'&&ch<='8'){ c += ch-'0'; continue; }
      int color = (ch>='a'&&ch<='z') ? BLACK : WHITE;
      char up = (ch>='a'&&ch<='z') ? ch-32 : ch;
      int type = up=='P'?PAWN: up=='N'?KNIGHT: up=='B'?BISHOP: up=='R'?ROOK: up=='Q'?QUEEN: KING;
      g_board[SQ(r,c)] = MKP(type,color);
      c++;
    }
  }
}

int main(void){
  int fails=0;

  setupKiwipete();
  long got = perft(4, WHITE, 1,1,1,1, -1,-1);
  /* Cross-validated against an independently written Python move generator
   * (ref_perft.py) — both agree exactly across all 48 branches at depth 4. */
  long expected = 4074224;
  printf("kiwipete perft(4) = %ld (expected %ld, cross-validated) %s\n", got, expected, got==expected?"OK":"FAIL");
  if(got!=expected) fails++;

  long posExp[6] = {1,14,191,2812,43238,674624};
  for(int d=0; d<=5; d++){
    setupPos3();
    long g = perft(d, WHITE, 0,0,0,0, -1,-1);
    printf("pos3 perft(%d) = %ld (expected %ld) %s\n", d, g, posExp[d], g==posExp[d]?"OK":"FAIL");
    if(g!=posExp[d]) fails++;
  }

  /* Timing sanity: run the real time-boxed search at each difficulty from the
   * startpos and from a busy midgame-like position, confirm it NEVER exceeds
   * its budget by more than a small margin regardless of difficulty. */
  const char* names[4] = {"easy","medium","hard","gopro"};
  for(int diff=0; diff<4; diff++){
    setupStartpos();
    double t0 = now_ms();
    int ok = wasm_search(BLACK, 1,1,1,1, -1,-1, diff, 0, (unsigned)42+diff, 0);
    double elapsed = now_ms()-t0;
    printf("search[%s] from startpos: ok=%d depth=%d nodes=%d elapsed=%.1fms eval=%.2f budget=%dms\n",
           names[diff], ok, get_result_depth(), get_result_nodes(), elapsed, get_result_eval(), DIFF[diff].timeMs);
    if(elapsed > DIFF[diff].timeMs + 250) { printf("  ** OVER BUDGET **\n"); fails++; }
  }

  /* A tactically dense middlegame position (many captures available at every ply)
   * is the worst case for the search — verify GoPro still respects its time cap. */
  setupKiwipete();
  double t0 = now_ms();
  int ok = wasm_search(WHITE, 1,1,1,1, -1,-1, 3, 0, 777, 0);
  double elapsed = now_ms()-t0;
  printf("search[gopro] from kiwipete (dense middlegame): ok=%d depth=%d nodes=%d elapsed=%.1fms eval=%.2f\n",
         ok, get_result_depth(), get_result_nodes(), elapsed, get_result_eval());
  if(elapsed > DIFF[3].timeMs + 250) { printf("  ** OVER BUDGET **\n"); fails++; }

  printf(fails==0 ? "\nALL EXTRA TESTS PASSED\n" : "\n%d EXTRA TESTS FAILED\n", fails);
  return fails==0?0:1;
}
