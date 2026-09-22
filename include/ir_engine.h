#ifndef IR_ENGINE_H
#define IR_ENGINE_H

#include <Arduino.h>
#include <vector>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

enum class IrSignalType {
    PARSED,
    RAW
};

struct IrButton {
    String name;
    IrSignalType type;
    // Parsed fields
    String protocol;
    uint64_t address;
    uint64_t command;
    uint16_t nbits;
    // Raw fields
    uint32_t frequency; // Hz, e.g. 38000
    float duty_cycle;   // e.g. 0.33
    std::vector<uint16_t> raw_data; // microseconds timing array
};

struct IrRemoteFile {
    String filepath;
    String name;
    std::vector<IrButton> buttons;
};

struct TvBGoneCode {
    const char* brand;
    decode_type_t type;
    uint64_t data;
    uint16_t nbits;
    uint32_t frequency;
};

class IREngine {
public:
    IREngine();
    void begin();
    void loop();

    // Transmission
    bool sendButton(const IrButton& btn);
    bool sendParsed(const String& protocol, uint64_t address, uint64_t command, uint16_t nbits);
    bool sendRaw(const uint16_t* timings, size_t count, uint16_t frequency = 38);

    // File IO (.ir Flipper / Bruce format)
    bool parseIrFile(const String& path, IrRemoteFile& remote);
    bool saveIrFile(const String& path, const IrRemoteFile& remote);
    bool appendButtonToIrFile(const String& path, const IrButton& btn);
    std::vector<String> listIrFiles();

    // Capture & Learning (IR READ & QUICK REMOTE)
    void startCapture();
    void stopCapture();
    bool isCapturing() const { return capturing; }
    bool checkCapturedSignal(IrButton& out_btn);
    bool analyzeRawToParsed(IrButton& btn);

    // TV-B-Gone
    void startTvBGone();
    void stopTvBGone();
    bool isTvBGoneRunning() const { return tv_bgone_running; }
    int getTvBGoneProgressPercent() const;
    int getTvBGoneCurrentIndex() const { return tv_bgone_idx; }
    int getTvBGoneTotalCount() const { return tv_bgone_total; }
    String getTvBGoneCurrentBrand() const { return tv_bgone_current_brand; }

    // Recent & Favorites
    void addRecent(const String& remote_name, const IrButton& btn);
    std::vector<String> getRecentList();
    void addFavorite(const String& remote_name, const String& button_name);
    void removeFavorite(const String& remote_name, const String& button_name);
    bool isFavorite(const String& remote_name, const String& button_name);
    std::vector<String> getFavoritesList();

    // Signal Lab & Diagnostics
    void startCarrierTest(uint32_t freq_hz);
    void stopCarrierTest();
    bool isCarrierTestActive() const { return carrier_test_active; }
    uint32_t getCarrierFreq() const { return carrier_freq; }

    // Helper conversion
    static decode_type_t strToDecodeType(const String& proto);
    static String decodeTypeToStr(decode_type_t type);

private:
    IRsend irsend;
    IRrecv irrecv;
    decode_results results;
    bool capturing;

    // TV-B-Gone state
    bool tv_bgone_running;
    size_t tv_bgone_idx;
    size_t tv_bgone_total;
    uint32_t tv_bgone_last_tx;
    String tv_bgone_current_brand;
    std::vector<TvBGoneCode> tv_bgone_codes;

    // Carrier test state
    bool carrier_test_active;
    uint32_t carrier_freq;

    void loadDefaultTvBGoneCodes();
    void ensureIrDirectory();
};

extern IREngine irEngine;

#endif // IR_ENGINE_H
