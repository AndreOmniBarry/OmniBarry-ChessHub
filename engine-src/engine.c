/* ============================================================
 * OmniBarry Chess Hub — native chess engine core (C, -> WASM)
 * ============================================================
 * Board: 8x8 flat array, index r*8+c. r=0 is Black's back rank,
 * r=7 is White's back rank (White pawns advance toward r=0),
 * matching the JS engine's convention exactly.
 *
 * Piece byte: 0 = empty; else (color<<3)|type.
 *   type: PAWN=1 KNIGHT=2 BISHOP=3 ROOK=4 QUEEN=5 KING=6
 *   color: WHITE=0 BLACK=1
 *
 * Search: minimax + alpha-beta + quiescence, in-place make/unmake
 * (zero heap allocation on the hot path), MVV-LVA move ordering,
 * time-boxed iterative deepening. A search that runs out of its
 * wall-clock budget unwinds via setjmp/longjmp back to the last
 * FULLY completed depth — it never returns a half-searched result,
 * and it never runs longer than the budget handed to it.
 * ============================================================ */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
  #define KEEPALIVE EMSCRIPTEN_KEEPALIVE
  static double now_ms(void){ return emscripten_get_now(); }
#else
  #include <time.h>
  #define KEEPALIVE
  static double now_ms(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return ts.tv_sec*1000.0 + ts.tv_nsec/1e6;
  }
#endif

typedef int8_t Piece;

#define PAWN 1
#define KNIGHT 2
#define BISHOP 3
#define ROOK 4
#define QUEEN 5
#define KING 6
#define WHITE 0
#define BLACK 1

#define PTYPE(p) ((p)&7)
#define PCOLOR(p) (((p)>>3)&1)
#define MKP(t,c) ((Piece)((t)|((c)<<3)))
#define OPP(c) (1-(c))
#define SQ(r,c) ((r)*8+(c))
#define MAX_MOVES 256

typedef struct { int8_t fr,fc,tr,tc,castle; } Move;
typedef struct { int wK,wQ,bK,bQ; } CR;
typedef struct {
  int fr,fc,tr,tc;
  Piece piece, captured;
  int isEP, epR, epC; Piece epCaptured;
  int isCastle, rookFromR, rookFromC, rookToR, rookToC; Piece rook;
} Undo;

static const int PIECE_VAL[7] = {0,100,320,330,500,900,20000};

static const int PST_P[64] = {
   0, 0, 0, 0, 0, 0, 0, 0, 50,50,50,50,50,50,50,50,
  10,10,20,30,30,20,10,10,  5, 5,10,25,25,10, 5, 5,
   0, 0, 0,20,20, 0, 0, 0,  5,-5,-10,0,0,-10,-5, 5,
   5,10,10,-20,-20,10,10, 5, 0, 0, 0, 0, 0, 0, 0, 0
};
static const int PST_N[64] = {
 -50,-40,-30,-30,-30,-30,-40,-50, -40,-20,  0,  0,  0,  0,-20,-40,
 -30,  0, 10, 15, 15, 10,  0,-30, -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30, -30,  5, 10, 15, 15, 10,  5,-30,
 -40,-20,  0,  5,  5,  0,-20,-40, -50,-40,-30,-30,-30,-30,-40,-50
};
static const int PST_B[64] = {
 -20,-10,-10,-10,-10,-10,-10,-20, -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0,  5, 10, 10,  5,  0,-10, -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0, 10, 10, 10, 10,  0,-10, -10, 10, 10, 10, 10, 10, 10,-10,
 -10,  5,  0,  0,  0,  0,  5,-10, -20,-10,-10,-10,-10,-10,-10,-20
};
static const int PST_R[64] = {
   0, 0, 0, 0, 0, 0, 0, 0,  5,10,10,10,10,10,10, 5,
  -5, 0, 0, 0, 0, 0, 0,-5, -5, 0, 0, 0, 0, 0, 0,-5,
  -5, 0, 0, 0, 0, 0, 0,-5, -5, 0, 0, 0, 0, 0, 0,-5,
  -5, 0, 0, 0, 0, 0, 0,-5,  0, 0, 0, 5, 5, 0, 0, 0
};
static const int PST_Q[64] = {
 -20,-10,-10,-5,-5,-10,-10,-20, -10,  0,  0, 0, 0,  0,  0,-10,
 -10,  0,  5, 5, 5,  5,  0,-10,  -5,  0,  5, 5, 5,  5,  0, -5,
   0,  0,  5, 5, 5,  5,  0, -5, -10,  5,  5, 5, 5,  5,  0,-10,
 -10,  0,  5, 0, 0,  0,  0,-10, -20,-10,-10,-5,-5,-10,-10,-20
};
static const int PST_K[64] = {
 -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
 -20,-30,-30,-40,-40,-30,-30,-20, -10,-20,-20,-20,-20,-20,-20,-10,
  20, 20,  0,  0,  0,  0, 20, 20,  20, 30, 10,  0,  0, 10, 30, 20
};
static const int* PST_OF(int type){
  switch(type){
    case PAWN: return PST_P; case KNIGHT: return PST_N; case BISHOP: return PST_B;
    case ROOK: return PST_R; case QUEEN: return PST_Q; default: return PST_K;
  }
}

static Piece g_board[64];

static inline int inb(int r,int c){ return r>=0&&r<8&&c>=0&&c<8; }

/* ── Pseudo-legal move generation ─────────────────────────────── */
static int pseudoMoves(Piece*bd, int r,int c, int epR,int epC, CR*cr, Move*out){
  Piece p = bd[SQ(r,c)];
  if(!p) return 0;
  int color = PCOLOR(p), type = PTYPE(p), n=0;

  if(type==PAWN){
    int dir = color==WHITE? -1:1;
    int sr = color==WHITE?6:1;
    if(inb(r+dir,c) && !bd[SQ(r+dir,c)]){
      out[n].fr=r;out[n].fc=c;out[n].tr=r+dir;out[n].tc=c;out[n].castle=0; n++;
      if(r==sr && !bd[SQ(r+2*dir,c)]){ out[n].fr=r;out[n].fc=c;out[n].tr=r+2*dir;out[n].tc=c;out[n].castle=0; n++; }
    }
    for(int k=0;k<2;k++){
      int dc = k==0?-1:1;
      if(!inb(r+dir,c+dc)) continue;
      Piece t = bd[SQ(r+dir,c+dc)];
      if(t && PCOLOR(t)!=color){ out[n].fr=r;out[n].fc=c;out[n].tr=r+dir;out[n].tc=c+dc;out[n].castle=0; n++; }
      else if(epR==r+dir && epC==c+dc){ out[n].fr=r;out[n].fc=c;out[n].tr=r+dir;out[n].tc=c+dc;out[n].castle=0; n++; }
    }
  } else if(type==KNIGHT){
    static const int kd[8][2]={{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    for(int i=0;i<8;i++){
      int nr=r+kd[i][0], nc=c+kd[i][1];
      if(inb(nr,nc)){ Piece t=bd[SQ(nr,nc)]; if(!t||PCOLOR(t)!=color){ out[n].fr=r;out[n].fc=c;out[n].tr=nr;out[n].tc=nc;out[n].castle=0; n++; } }
    }
  } else if(type==BISHOP){
    static const int dirs[4][2]={{-1,-1},{-1,1},{1,-1},{1,1}};
    for(int i=0;i<4;i++){ int dr=dirs[i][0],dc=dirs[i][1],nr=r+dr,nc=c+dc;
      while(inb(nr,nc)){ Piece t=bd[SQ(nr,nc)];
        if(t){ if(PCOLOR(t)!=color){out[n].fr=r;out[n].fc=c;out[n].tr=nr;out[n].tc=nc;out[n].castle=0;n++;} break; }
        out[n].fr=r;out[n].fc=c;out[n].tr=nr;out[n].tc=nc;out[n].castle=0;n++; nr+=dr;nc+=dc;
      }
    }
  } else if(type==ROOK){
    static const int dirs[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
    for(int i=0;i<4;i++){ int dr=dirs[i][0],dc=dirs[i][1],nr=r+dr,nc=c+dc;
      while(inb(nr,nc)){ Piece t=bd[SQ(nr,nc)];
        if(t){ if(PCOLOR(t)!=color){out[n].fr=r;out[n].fc=c;out[n].tr=nr;out[n].tc=nc;out[n].castle=0;n++;} break; }
        out[n].fr=r;out[n].fc=c;out[n].tr=nr;out[n].tc=nc;out[n].castle=0;n++; nr+=dr;nc+=dc;
      }
    }
  } else if(type==QUEEN){
    static const int dirs[8][2]={{-1,-1},{-1,1},{1,-1},{1,1},{-1,0},{1,0},{0,-1},{0,1}};
    for(int i=0;i<8;i++){ int dr=dirs[i][0],dc=dirs[i][1],nr=r+dr,nc=c+dc;
      while(inb(nr,nc)){ Piece t=bd[SQ(nr,nc)];
        if(t){ if(PCOLOR(t)!=color){out[n].fr=r;out[n].fc=c;out[n].tr=nr;out[n].tc=nc;out[n].castle=0;n++;} break; }
        out[n].fr=r;out[n].fc=c;out[n].tr=nr;out[n].tc=nc;out[n].castle=0;n++; nr+=dr;nc+=dc;
      }
    }
  } else if(type==KING){
    static const int kg[8][2]={{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
    for(int i=0;i<8;i++){
      int nr=r+kg[i][0], nc=c+kg[i][1];
      if(inb(nr,nc)){ Piece t=bd[SQ(nr,nc)]; if(!t||PCOLOR(t)!=color){ out[n].fr=r;out[n].fc=c;out[n].tr=nr;out[n].tc=nc;out[n].castle=0; n++; } }
    }
    int backR = color==WHITE?7:0;
    if(r==backR && c==4){
      int K = color==WHITE? cr->wK : cr->bK;
      int Q = color==WHITE? cr->wQ : cr->bQ;
      Piece kr = bd[SQ(backR,7)], qr = bd[SQ(backR,0)];
      if(K && !bd[SQ(backR,5)] && !bd[SQ(backR,6)] && kr && PTYPE(kr)==ROOK && PCOLOR(kr)==color){
        out[n].fr=r;out[n].fc=c;out[n].tr=backR;out[n].tc=6;out[n].castle=1; n++;
      }
      if(Q && !bd[SQ(backR,3)] && !bd[SQ(backR,2)] && !bd[SQ(backR,1)] && qr && PTYPE(qr)==ROOK && PCOLOR(qr)==color){
        out[n].fr=r;out[n].fc=c;out[n].tr=backR;out[n].tc=2;out[n].castle=2; n++;
      }
    }
  }
  return n;
}

static int findKing(Piece*bd,int color,int*outR,int*outC){
  for(int r=0;r<8;r++)for(int c=0;c<8;c++){ Piece p=bd[SQ(r,c)]; if(p && PCOLOR(p)==color && PTYPE(p)==KING){ *outR=r;*outC=c; return 1; } }
  return 0;
}
static int isInCheck(Piece*bd,int color,int epR,int epC,CR*cr){
  int kr,kc;
  if(!findKing(bd,color,&kr,&kc)) return 1;
  int oc = OPP(color);
  Move buf[32];
  for(int r=0;r<8;r++)for(int c=0;c<8;c++){
    Piece p=bd[SQ(r,c)];
    if(p && PCOLOR(p)==oc){
      int n=pseudoMoves(bd,r,c,epR,epC,cr,buf);
      for(int i=0;i<n;i++) if(buf[i].tr==kr && buf[i].tc==kc) return 1;
    }
  }
  return 0;
}
static int isAttacked(Piece*bd,int r,int c,int byColor,int epR,int epC,CR*cr){
  Move buf[32];
  for(int rr=0;rr<8;rr++)for(int cc=0;cc<8;cc++){
    Piece p=bd[SQ(rr,cc)];
    if(p && PCOLOR(p)==byColor){
      int n=pseudoMoves(bd,rr,cc,epR,epC,cr,buf);
      for(int i=0;i<n;i++) if(buf[i].tr==r && buf[i].tc==c) return 1;
    }
  }
  return 0;
}

/* ── In-place make/unmake — no allocation ─────────────────────── */
static void makeMove(Piece*bd, int fr,int fc,int tr,int tc, int promoType, Undo*u){
  Piece piece = bd[SQ(fr,fc)];
  Piece captured = bd[SQ(tr,tc)];
  int isEP=0, epR=-1, epC=-1; Piece epCaptured=0;
  if(PTYPE(piece)==PAWN && !captured && fc!=tc){
    isEP=1; epR=fr; epC=tc; epCaptured=bd[SQ(epR,epC)]; captured=epCaptured; bd[SQ(epR,epC)]=0;
  }
  bd[SQ(tr,tc)] = piece;
  bd[SQ(fr,fc)] = 0;
  int isCastle=0, rfr=0,rfc=0,rtr=0,rtc=0; Piece rook=0;
  if(PTYPE(piece)==KING && (tc-fc==2 || fc-tc==2)){
    isCastle=1; rfr=fr; rtr=fr;
    if(tc==6){ rfc=7; rtc=5; } else { rfc=0; rtc=3; }
    rook = bd[SQ(rfr,rfc)];
    bd[SQ(rtr,rtc)] = rook;
    bd[SQ(rfr,rfc)] = 0;
  }
  if(PTYPE(piece)==PAWN && (tr==0 || tr==7)){
    bd[SQ(tr,tc)] = MKP(promoType?promoType:QUEEN, PCOLOR(piece));
  }
  u->fr=fr;u->fc=fc;u->tr=tr;u->tc=tc;u->piece=piece;u->captured=captured;
  u->isEP=isEP;u->epR=epR;u->epC=epC;u->epCaptured=epCaptured;
  u->isCastle=isCastle;u->rookFromR=rfr;u->rookFromC=rfc;u->rookToR=rtr;u->rookToC=rtc;u->rook=rook;
}
static void unmakeMove(Piece*bd, Undo*u){
  bd[SQ(u->fr,u->fc)] = u->piece;
  bd[SQ(u->tr,u->tc)] = u->isEP ? 0 : u->captured;
  if(u->isEP) bd[SQ(u->epR,u->epC)] = u->epCaptured;
  if(u->isCastle){ bd[SQ(u->rookFromR,u->rookFromC)] = u->rook; bd[SQ(u->rookToR,u->rookToC)] = 0; }
}
static CR makeCR(CR*cr, Piece piece, int fr,int fc,int tr,int tc, Piece captured){
  CR prev = *cr;
  int color = PCOLOR(piece);
  if(PTYPE(piece)==KING){ if(color==WHITE){cr->wK=0;cr->wQ=0;} else {cr->bK=0;cr->bQ=0;} }
  if(PTYPE(piece)==ROOK){
    int br = color==WHITE?7:0;
    if(fr==br && fc==7){ if(color==WHITE) cr->wK=0; else cr->bK=0; }
    if(fr==br && fc==0){ if(color==WHITE) cr->wQ=0; else cr->bQ=0; }
  }
  if(captured && PTYPE(captured)==ROOK){
    int cc = PCOLOR(captured), br2 = cc==WHITE?7:0;
    if(tr==br2 && tc==7){ if(cc==WHITE) cr->wK=0; else cr->bK=0; }
    if(tr==br2 && tc==0){ if(cc==WHITE) cr->wQ=0; else cr->bQ=0; }
  }
  return prev;
}
static void unmakeCR(CR*cr, CR prev){ *cr = prev; }
static void makeEP(int fr,int fc,int tr,int tc,Piece piece,int*outR,int*outC){
  (void)fc;
  if(PTYPE(piece)==PAWN && (tr-fr==2 || fr-tr==2)){ *outR=(fr+tr)/2; *outC=fc; }
  else { *outR=-1; *outC=-1; }
}

static int legalMovesFor(Piece*bd, int r,int c, int epR,int epC, CR*cr, Move*out){
  Piece piece = bd[SQ(r,c)];
  if(!piece) return 0;
  Move pseudo[32];
  int npseudo = pseudoMoves(bd,r,c,epR,epC,cr,pseudo);
  int n=0;
  for(int i=0;i<npseudo;i++){
    Move mv = pseudo[i];
    if(PTYPE(piece)==KING && (mv.tc-c==2 || c-mv.tc==2)){
      int step = mv.tc>c?1:-1, safe=1;
      for(int sq=c; safe && sq!=(mv.tc+step); sq+=step){
        Piece savedSq = bd[SQ(r,sq)], savedC = bd[SQ(r,c)];
        bd[SQ(r,sq)] = piece;
        if(sq!=c) bd[SQ(r,c)] = 0;
        if(isAttacked(bd,r,sq,OPP(PCOLOR(piece)),epR,epC,cr)) safe=0;
        bd[SQ(r,c)] = savedC;
        bd[SQ(r,sq)] = savedSq;
      }
      if(!safe) continue;
    }
    Piece targetBefore = bd[SQ(mv.tr,mv.tc)];
    CR prevCR = makeCR(cr, piece, r,c, mv.tr,mv.tc, targetBefore);
    Undo undo; makeMove(bd, r,c, mv.tr,mv.tc, QUEEN, &undo);
    int inCheck = isInCheck(bd, PCOLOR(piece), -1,-1, cr);
    unmakeMove(bd,&undo);
    unmakeCR(cr, prevCR);
    if(!inCheck){ out[n]=mv; n++; }
  }
  return n;
}
static int allLegalMoves(Piece*bd, int color, int epR,int epC, CR*cr, Move*out){
  int n=0; Move buf[32];
  for(int r=0;r<8;r++)for(int c=0;c<8;c++){
    Piece p = bd[SQ(r,c)];
    if(!p || PCOLOR(p)!=color) continue;
    int cnt = legalMovesFor(bd,r,c,epR,epC,cr,buf);
    for(int i=0;i<cnt && n<MAX_MOVES;i++){ out[n].fr=r; out[n].fc=c; out[n].tr=buf[i].tr; out[n].tc=buf[i].tc; out[n].castle=buf[i].castle; n++; }
  }
  return n;
}

static int pieceCountFn(Piece*bd){ int n=0; for(int i=0;i<64;i++) if(bd[i]) n++; return n; }

/* personality: 0=solid 1=aggressive 2=positional */
static int evaluate(Piece*bd, int personality){
  int score=0, wB=0,bB=0;
  for(int r=0;r<8;r++)for(int c=0;c<8;c++){
    Piece p=bd[SQ(r,c)]; if(!p) continue;
    int color=PCOLOR(p), type=PTYPE(p);
    int idx = color==WHITE? r*8+c : (7-r)*8+c;
    int val = PIECE_VAL[type] + PST_OF(type)[idx];
    if(personality==1){
      if(color==BLACK && r>=4) val+=8;
      if(type==ROOK){ int open=1; for(int rr=0;rr<8;rr++){ Piece q=bd[SQ(rr,c)]; if(q && PTYPE(q)==PAWN) open=0; } if(open) val+=15; }
    } else if(personality==2){
      if(type==PAWN && (c==3||c==4) && r>=3 && r<=5) val+=12;
      if(type==BISHOP){ if(color==WHITE) wB++; else bB++; }
    } else {
      if((type==KNIGHT||type==BISHOP) && color==BLACK && r!=0) val+=6;
      if((type==KNIGHT||type==BISHOP) && color==WHITE && r!=7) val+=6;
    }
    score += color==WHITE? val : -val;
  }
  int bp = personality==2? 40:20;
  if(wB>=2) score+=bp; if(bB>=2) score-=bp;
  return score;
}

static void shuffleMoves(Move*arr,int n){
  for(int i=n-1;i>0;i--){ int j = rand()%(i+1); Move t=arr[i]; arr[i]=arr[j]; arr[j]=t; }
}
static int movePriority(Piece*bd, Move*mv){
  Piece target = bd[SQ(mv->tr,mv->tc)];
  Piece attacker = bd[SQ(mv->fr,mv->fc)];
  int pr=0;
  if(target) pr += 1000 + PIECE_VAL[PTYPE(target)]*10 - PIECE_VAL[PTYPE(attacker)];
  if(attacker && PTYPE(attacker)==PAWN && (mv->tr==0||mv->tr==7)) pr += 900;
  return pr;
}
/* Shuffle first (keeps AI variety among equal moves), then a stable
 * insertion sort surfaces captures/promotions first for alpha-beta. */
static void orderMoves(Move*arr,int n, Piece*bd){
  shuffleMoves(arr,n);
  static int pr[MAX_MOVES];
  for(int i=0;i<n;i++) pr[i]=movePriority(bd,&arr[i]);
  for(int i=1;i<n;i++){
    Move mv=arr[i]; int p=pr[i]; int j=i-1;
    while(j>=0 && pr[j]<p){ arr[j+1]=arr[j]; pr[j+1]=pr[j]; j--; }
    arr[j+1]=mv; pr[j+1]=p;
  }
}

/* ── Time-boxed search ─────────────────────────────────────────── */
static jmp_buf g_abortEnv;
static long g_nodes;
static double g_deadline;

static int timeUp(void){
  g_nodes++;
  if((g_nodes & 1023)==0) return now_ms() > g_deadline;
  return 0;
}

static int quiescence(Piece*bd,int alpha,int beta,int maximizing,int epR,int epC,CR*cr,int qdepth,int personality){
  if(timeUp()) longjmp(g_abortEnv,1);
  int sp = evaluate(bd,personality);
  if(qdepth<=0) return sp;
  if(maximizing){ if(sp>=beta) return beta; if(sp>alpha) alpha=sp; }
  else { if(sp<=alpha) return alpha; if(sp<beta) beta=sp; }
  int color = maximizing? WHITE:BLACK;
  Move all[MAX_MOVES];
  int nAll = allLegalMoves(bd,color,epR,epC,cr,all);
  Move caps[MAX_MOVES]; int nCaps=0;
  for(int i=0;i<nAll;i++){
    Move mv=all[i];
    Piece target = bd[SQ(mv.tr,mv.tc)];
    Piece attacker = bd[SQ(mv.fr,mv.fc)];
    if(target!=0 || (PTYPE(attacker)==PAWN && (mv.tr==0||mv.tr==7))) caps[nCaps++]=mv;
  }
  orderMoves(caps,nCaps,bd);
  int best=sp;
  for(int i=0;i<nCaps;i++){
    Move mv=caps[i];
    Piece p = bd[SQ(mv.fr,mv.fc)];
    Piece targetBefore = bd[SQ(mv.tr,mv.tc)];
    CR prevCR = makeCR(cr,p,mv.fr,mv.fc,mv.tr,mv.tc,targetBefore);
    int nepR,nepC; makeEP(mv.fr,mv.fc,mv.tr,mv.tc,p,&nepR,&nepC);
    Undo undo; makeMove(bd,mv.fr,mv.fc,mv.tr,mv.tc,QUEEN,&undo);
    int val = quiescence(bd,alpha,beta,!maximizing,nepR,nepC,cr,qdepth-1,personality);
    unmakeMove(bd,&undo);
    unmakeCR(cr,prevCR);
    if(maximizing){ if(val>best) best=val; if(best>alpha) alpha=best; if(beta<=alpha) break; }
    else { if(val<best) best=val; if(best<beta) beta=best; if(beta<=alpha) break; }
  }
  return best;
}

static int minimax(Piece*bd,int depth,int alpha,int beta,int maximizing,int epR,int epC,CR*cr,int useQ,int qdepth,int personality){
  if(timeUp()) longjmp(g_abortEnv,1);
  if(depth==0) return useQ? quiescence(bd,alpha,beta,maximizing,epR,epC,cr,qdepth,personality) : evaluate(bd,personality);
  int color = maximizing?WHITE:BLACK;
  Move moves[MAX_MOVES];
  int nmoves = allLegalMoves(bd,color,epR,epC,cr,moves);
  if(nmoves==0){
    if(isInCheck(bd,color,epR,epC,cr)){ int mate=99000-depth; return maximizing? -mate: mate; }
    return 0;
  }
  orderMoves(moves,nmoves,bd);
  int best = maximizing? -1000000: 1000000;
  for(int i=0;i<nmoves;i++){
    Move mv=moves[i];
    Piece p = bd[SQ(mv.fr,mv.fc)];
    Piece targetBefore = bd[SQ(mv.tr,mv.tc)];
    CR prevCR = makeCR(cr,p,mv.fr,mv.fc,mv.tr,mv.tc,targetBefore);
    int nepR,nepC; makeEP(mv.fr,mv.fc,mv.tr,mv.tc,p,&nepR,&nepC);
    Undo undo; makeMove(bd,mv.fr,mv.fc,mv.tr,mv.tc,QUEEN,&undo);
    int val = minimax(bd,depth-1,alpha,beta,!maximizing,nepR,nepC,cr,useQ,qdepth,personality);
    unmakeMove(bd,&undo);
    unmakeCR(cr,prevCR);
    if(maximizing){ if(val>best) best=val; if(best>alpha) alpha=best; if(beta<=alpha) break; }
    else { if(val<best) best=val; if(best<beta) beta=best; if(beta<=alpha) break; }
  }
  return best;
}

typedef struct { int depth, noise, pool, blunder_pct, useQ, qdepth, timeMs; } DiffCfg;
static const DiffCfg DIFF[4] = {
  {2,70,100,22,0,0,350},
  {3,25,70,6, 0,0,600},
  {4,0, 40,0, 1,4,1100},
  {5,0, 20,0, 1,4,2200},
};

static int g_outFr,g_outFc,g_outTr,g_outTc,g_outDepth,g_outBook,g_outBlunder,g_outFallback;
static long g_outNodes;
static double g_outElapsed,g_outEval;

/**
 * Runs the full time-boxed search against g_board (populate it first
 * via get_board_ptr()). Returns 1 with a move written to g_out*, or
 * 0 if the side to move has no legal moves.
 */
KEEPALIVE
int wasm_search(int aiColor, int wK,int wQ,int bK,int bQ, int epR,int epC,
                 int difficulty, int personality, unsigned int seed, int timeBudgetMsOverride){
  srand(seed);
  double t0 = now_ms();
  g_nodes = 0;
  g_outBook=0; g_outBlunder=0; g_outFallback=0;

  CR cr; cr.wK=wK; cr.wQ=wQ; cr.bK=bK; cr.bQ=bQ;
  DiffCfg cfg = DIFF[(difficulty>=0 && difficulty<4)?difficulty:1];

  Move root[MAX_MOVES];
  int nroot = allLegalMoves(g_board, aiColor, epR,epC, &cr, root);
  if(nroot==0) return 0;

  if(cfg.blunder_pct>0 && (int)(rand()%100) < cfg.blunder_pct){
    Move mv = root[rand()%nroot];
    g_outFr=mv.fr; g_outFc=mv.fc; g_outTr=mv.tr; g_outTc=mv.tc;
    g_outDepth=0; g_outElapsed=now_ms()-t0; g_outNodes=0; g_outEval=0; g_outBlunder=1;
    return 1;
  }

  orderMoves(root,nroot,g_board);

  int maximizingAI = (aiColor==WHITE);
  int budget = timeBudgetMsOverride>0? timeBudgetMsOverride : cfg.timeMs;
  g_deadline = t0 + budget;

  static int scores[MAX_MOVES], bestScores[MAX_MOVES];
  int lastValid = 0, depthReached = 0;

  for(int depth=1; depth<=cfg.depth; depth++){
    int aborted = 0;
    if(setjmp(g_abortEnv) != 0){
      aborted = 1;
    } else {
      for(int k=0;k<nroot;k++){
        Move mv = root[k];
        Piece p = g_board[SQ(mv.fr,mv.fc)];
        Piece targetBefore = g_board[SQ(mv.tr,mv.tc)];
        CR prevCR = makeCR(&cr,p,mv.fr,mv.fc,mv.tr,mv.tc,targetBefore);
        int nepR,nepC; makeEP(mv.fr,mv.fc,mv.tr,mv.tc,p,&nepR,&nepC);
        Undo undo; makeMove(g_board,mv.fr,mv.fc,mv.tr,mv.tc,QUEEN,&undo);
        int score = minimax(g_board,depth-1,-1000000,1000000,!maximizingAI,nepR,nepC,&cr,cfg.useQ,cfg.qdepth,personality);
        unmakeMove(g_board,&undo);
        unmakeCR(&cr,prevCR);
        scores[k] = score;
      }
    }
    if(aborted) break;
    for(int k=0;k<nroot;k++) bestScores[k]=scores[k];
    lastValid = 1;
    depthReached = depth;
    if(now_ms() > g_deadline) break;
  }

  if(!lastValid){
    Move mv = root[0];
    g_outFr=mv.fr; g_outFc=mv.fc; g_outTr=mv.tr; g_outTc=mv.tc;
    g_outDepth=0; g_outElapsed=now_ms()-t0; g_outNodes=g_nodes; g_outEval=0; g_outFallback=1;
    return 1;
  }

  if(cfg.noise>0){
    for(int k=0;k<nroot;k++){
      double r1=(double)rand()/RAND_MAX, r2=(double)rand()/RAND_MAX, r3=(double)rand()/RAND_MAX;
      bestScores[k] += (int)((r1+r2+r3-1.5) * (cfg.noise*0.67));
    }
  }

  int pc = pieceCountFn(g_board);
  double phaseScale = pc>24? 1.4 : pc>14? 1.0 : 0.6;
  int threshold = (int)(cfg.pool * phaseScale);

  int bestScore;
  if(maximizingAI){ bestScore=-1000000; for(int k=0;k<nroot;k++) if(bestScores[k]>bestScore) bestScore=bestScores[k]; }
  else { bestScore=1000000; for(int k=0;k<nroot;k++) if(bestScores[k]<bestScore) bestScore=bestScores[k]; }

  int poolIdx[MAX_MOVES], poolN=0;
  for(int k=0;k<nroot;k++){
    int inPool = maximizingAI? (bestScores[k] >= bestScore-threshold) : (bestScores[k] <= bestScore+threshold);
    if(inPool) poolIdx[poolN++]=k;
  }
  int pick = poolN>0? poolIdx[rand()%poolN] : 0;
  Move mv = root[pick];

  g_outFr=mv.fr; g_outFc=mv.fc; g_outTr=mv.tr; g_outTc=mv.tc;
  g_outDepth=depthReached; g_outElapsed=now_ms()-t0; g_outNodes=g_nodes; g_outEval = bestScore/100.0;
  return 1;
}

KEEPALIVE uint8_t* get_board_ptr(void){ return (uint8_t*)g_board; }
KEEPALIVE int get_result_fr(void){ return g_outFr; }
KEEPALIVE int get_result_fc(void){ return g_outFc; }
KEEPALIVE int get_result_tr(void){ return g_outTr; }
KEEPALIVE int get_result_tc(void){ return g_outTc; }
KEEPALIVE int get_result_depth(void){ return g_outDepth; }
KEEPALIVE double get_result_elapsed(void){ return g_outElapsed; }
KEEPALIVE double get_result_eval(void){ return g_outEval; }
KEEPALIVE int get_result_nodes(void){ return (int)g_outNodes; }
KEEPALIVE int get_result_blunder(void){ return g_outBlunder; }
KEEPALIVE int get_result_fallback(void){ return g_outFallback; }

/* perft — move-generation correctness check (no promotion-choice multiplicity,
 * since this engine always auto-queens; matches standard perft values exactly
 * at depths before any pawn reaches the last rank). */
KEEPALIVE
long perft(int depth, int color, int wK,int wQ,int bK,int bQ,int epR,int epC){
  CR cr; cr.wK=wK;cr.wQ=wQ;cr.bK=bK;cr.bQ=bQ;
  if(depth==0) return 1;
  Move moves[MAX_MOVES];
  int n = allLegalMoves(g_board, color, epR,epC, &cr, moves);
  if(depth==1) return n;
  long total=0;
  for(int i=0;i<n;i++){
    Move mv=moves[i];
    Piece p = g_board[SQ(mv.fr,mv.fc)];
    Piece targetBefore = g_board[SQ(mv.tr,mv.tc)];
    CR prevCR = makeCR(&cr,p,mv.fr,mv.fc,mv.tr,mv.tc,targetBefore);
    int nepR,nepC; makeEP(mv.fr,mv.fc,mv.tr,mv.tc,p,&nepR,&nepC);
    Undo undo; makeMove(g_board,mv.fr,mv.fc,mv.tr,mv.tc,QUEEN,&undo);
    total += perft(depth-1, OPP(color), cr.wK,cr.wQ,cr.bK,cr.bQ, nepR,nepC);
    unmakeMove(g_board,&undo);
    unmakeCR(&cr,prevCR);
  }
  return total;
}

#ifdef PERFT_MAIN
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
/* Kiwipete: r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - */
static void setupKiwipete(void){
  memset(g_board,0,sizeof(g_board));
  const char* rows[8] = {
    "r3k2r","p1ppqpb1","bn2pnp1","3PN3","1p2P3","2N2Q1p","PPPBBPPP","R3K2R"
  };
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
  setupStartpos();
  long expected1[6] = {1,20,400,8902,197281,4865609};
  for(int d=0; d<=5; d++){
    setupStartpos();
    long got = perft(d, WHITE, 1,1,1,1, -1,-1);
    int ok = got==expected1[d];
    printf("startpos perft(%d) = %ld (expected %ld) %s\n", d, got, expected1[d], ok?"OK":"FAIL");
    if(!ok) fails++;
  }
  long expectedKP[4] = {1,48,2039,97862};
  for(int d=0; d<=3; d++){
    setupKiwipete();
    long got = perft(d, WHITE, 1,1,1,1, -1,-1);
    int ok = got==expectedKP[d];
    printf("kiwipete perft(%d) = %ld (expected %ld) %s\n", d, got, expectedKP[d], ok?"OK":"FAIL");
    if(!ok) fails++;
  }
  printf(fails==0 ? "\nALL PERFT TESTS PASSED\n" : "\n%d PERFT TESTS FAILED\n", fails);
  return fails==0 ? 0 : 1;
}
#endif
