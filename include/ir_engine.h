#ifndef IR_ENGINE_H
#define IR_ENGINE_H

#include <Arduino.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include "hw_config.h"

#define MAX_IR_BUTTONS 24
#define MAX_RAW_TIMINGS 512

enum class IrSignalType {
    PARSED,
    RAW
};

struct IrButton {
    String name;
    IrSignalType type;
    decode_type_t protocol;
    String protocol_name;
    uint32_t address;
    uint32_t command;
    uint16_t bits;
    uint16_t frequency;
    float duty_cycle;
    uint16_t raw_data[MAX_RAW_TIMINGS];
    uint16_t raw_len;

    IrButton() : type(IrSignalType::PARSED), protocol(UNKNOWN), protocol_name("UNKNOWN"), address(0), command(0), bits(0), frequency(38000), duty_cycle(0.33f), raw_len(0) {}
};

struct IrRemote {
    String name;
    String filepath;
    IrButton buttons[MAX_IR_BUTTONS];
    uint8_t button_count;
    bool is_favorite;

    IrRemote() : button_count(0), is_favorite(false) {}
};

struct TvBGoneCode {
    decode_type_t protocol;
    uint64_t code;
    uint16_t bits;
    uint16_t frequency;
};

class IREngine {
public:
    IREngine();
    void begin();
    void loop();

    void sendSignal(const IrButton& btn);
    void startReceiver();
    void stopReceiver();
    bool isReceiverActive() const { return rx_active; }

    bool captureSignal(IrButton& btn, uint32_t timeout_ms = 5000);
    bool checkLiveSignal(IrButton& btn);
    bool reanalyzeRawToParsed(IrButton& btn);

    bool loadRemoteFile(const String& path, IrRemote& remote);
    bool saveRemoteFile(const String& path, const IrRemote& remote);

    void toggleFavorite(const String& remote_path);
    bool isFavorite(const String& remote_path);
    void addRecent(const String& remote_path);
    int getRecentCount() const { return recent_count; }
    String getRecentPath(int idx) const;

    void startTvBGone();
    void stopTvBGone();
    bool isTvBGoneRunning() const { return tvbg_running; }
    int getTvBGoneProgress() const { return tvbg_current_idx; }
    int getTvBGoneTotal() const { return tvbg_total_codes; }

    void runCarrierTest(uint16_t freq_hz, uint32_t duration_ms = 1000);
    struct RxDiagInfo {
        uint32_t carrier_freq;
        uint16_t pulse_count;
        uint16_t min_pulse_us;
        uint16_t max_pulse_us;
        String protocol_str;
    };
    RxDiagInfo getRxDiag();

private:
    IRrecv ir_recv;
    IRsend ir_send;
    bool rx_active;

    bool tvbg_running;
    int tvbg_current_idx;
    int tvbg_total_codes;
    uint32_t tvbg_last_tx_time;

    String recent_paths[8];
    uint8_t recent_count;

    void parseRawString(const String& raw_str, IrButton& btn);
    String serializeRawString(const IrButton& btn);
};

extern IREngine irEngine;

#endif // IR_ENGINE_H
