void _start() {
    volatile unsigned long long x = 0;
    // Spin for a while so we can see if it runs
    for (volatile unsigned long long i = 0; i < 500000000; i++) {
        x += i;
    }
    // Infinite loop so we can observe it's running
    while(1) {}
}
