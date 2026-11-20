/* Independent C compiler fixture, not the hand-authored demo instruction stream. */
static volatile unsigned rounds=9;
static __attribute__((noinline)) unsigned fib(unsigned n){unsigned a=0,b=1;while(n--){unsigned next=a+b;a=b;b=next;}return a;}
int entry(void){return (int)fib(rounds);}
