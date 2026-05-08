#include <stdio.h>
#include <stdint.h>

int main() {
    // Read TS_CAL1 from flash address 0x1FFF7568
    const uint16_t *ts_cal1_addr = (const uint16_t *)0x1FFF7568UL;
    uint16_t ts_cal1_value = *ts_cal1_addr;
    
    printf("TS_CAL1 address: 0x%p\n", (void *)ts_cal1_addr);
    printf("TS_CAL1 value: %u\n", ts_cal1_value);
    
    // Check against typical range
    if (ts_cal1_value >= 1000 && ts_cal1_value <= 1100) {
        printf("Value is in expected range for 30°C\n");
    } else {
        printf("WARNING: Value seems off for 30°C baseline\n");
    }
    
    return 0;
}
