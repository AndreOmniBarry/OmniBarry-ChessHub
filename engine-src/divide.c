#include "engine.c"
#include <stdio.h>

static void setupFEN(const char* rows[8]){
  memset(g_board,0,sizeof(g_board));
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

int main(int argc, char** argv){
  const char* rows[8] = { "r3k2r","p1ppqpb1","bn2pnp1","3PN3","1p2P3","2N2Q1p","PPPBBPPP","R3K2R" };
  setupFEN(rows);
  int depth = argc>1 ? atoi(argv[1]) : 4;
  CR cr; cr.wK=1;cr.wQ=1;cr.bK=1;cr.bQ=1;
  Move root[MAX_MOVES];
  int n = allLegalMoves(g_board, WHITE, -1,-1, &cr, root);
  const char* files="abcdefgh"; const char* ranks="87654321";
  long total=0;
  for(int i=0;i<n;i++){
    Move mv=root[i];
    Piece p = g_board[SQ(mv.fr,mv.fc)];
    Piece targetBefore = g_board[SQ(mv.tr,mv.tc)];
    CR prevCR = makeCR(&cr,p,mv.fr,mv.fc,mv.tr,mv.tc,targetBefore);
    int nepR,nepC; makeEP(mv.fr,mv.fc,mv.tr,mv.tc,p,&nepR,&nepC);
    Undo undo; makeMove(g_board,mv.fr,mv.fc,mv.tr,mv.tc,QUEEN,&undo);
    long cnt = perft(depth-1, BLACK, cr.wK,cr.wQ,cr.bK,cr.bQ, nepR,nepC);
    unmakeMove(g_board,&undo);
    unmakeCR(&cr,prevCR);
    printf("%c%c%c%c: %ld\n", files[mv.fc],ranks[mv.fr],files[mv.tc],ranks[mv.tr], cnt);
    total += cnt;
  }
  printf("total: %ld\n", total);
  return 0;
}
