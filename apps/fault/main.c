/**
 * @file apps/fault/main.c
 * @brief Trigger a user-mode divide-by-zero to verify fault isolation.
 */
int main() {
    volatile int x = 0;
    volatile int y = 1 / x;
    (void)y;
    return 0;
}
