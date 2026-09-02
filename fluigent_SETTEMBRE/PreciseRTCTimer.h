class PreciseRTCTimer {
public:
    struct TimeStamp {
        uint8_t hours;
        uint8_t minutes;
        uint8_t seconds;
        uint16_t milliseconds;
        uint32_t absoluteMs;
    };

    PreciseRTCTimer(RTCZero& rtcRef) : rtc(rtcRef) {}

    void begin() {
        rtc.begin(); // Assicurati che sia inizializzato
        lastSecond = rtc.getSeconds();
        millisSync = millis();
        
        // Calcolo iniziale per evitare il delta anomalo al primo loop
        previousAbsoluteMs = (uint32_t)rtc.getHours() * 3600000UL +
                             (uint32_t)rtc.getMinutes() * 60000UL +
                             (uint32_t)lastSecond * 1000UL;
    }

    TimeStamp update() {
        uint32_t nowMs = millis();
        uint8_t s = rtc.getSeconds();

        // Sincronizzazione: se il secondo è cambiato, resetta il riferimento millis
        if (s != lastSecond) {
            millisSync = nowMs;
            lastSecond = s;
        }

        uint8_t h = rtc.getHours();
        uint8_t m = rtc.getMinutes();
        
        // Calcoliamo i millisecondi trascorsi dall'ultimo cambio di secondo
        uint32_t diff = nowMs - millisSync;
        uint16_t msPart = (uint16_t)(diff % 1000);

        currentTime.hours = h;
        currentTime.minutes = m;
        currentTime.seconds = s;
        currentTime.milliseconds = msPart;
        currentTime.absoluteMs = (uint32_t)h * 3600000UL +
                                 (uint32_t)m * 60000UL +
                                 (uint32_t)s * 1000UL +
                                 msPart;
        return currentTime;
    }

    uint32_t getDelta() {
        uint32_t current = currentTime.absoluteMs;
        uint32_t delta = 0;

        if (current >= previousAbsoluteMs) {
            delta = current - previousAbsoluteMs;
        } else {
            // Gestione rollover mezzanotte (24h in ms = 86400000)
            delta = 100;//(86400000UL - previousAbsoluteMs) + current;
        }

        previousAbsoluteMs = current;
        return constrain(delta,0,200);
    }

private:
    RTCZero& rtc;
    uint32_t millisSync = 0;
    uint8_t lastSecond = 0;
    uint32_t previousAbsoluteMs = 0;
    TimeStamp currentTime;
};