//----------------------------------------------------------------------------------
//  AutoZYCParse.mod2 — RPP/MIDI 解析加速 + 表达式求值模块
//  For AviUtl ExEdit2 (x64)
//----------------------------------------------------------------------------------
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cctype>
#include <cstdint>

#include "aviutl2_sdk/module2.h"

struct Event {
    double position_sec;
    double length_sec;
    double pitch_shift;
    double velocity;
    int track;
    int event_type;
};

struct TrackMeta {
    double min_pitch;
    double max_pitch;
    int item_count;
};

static std::unordered_map<int, std::vector<Event>> g_events;
static std::unordered_map<int, TrackMeta> g_track_meta;
static int g_track_count = 0;

static void clear_state() {
    g_events.clear();
    g_track_meta.clear();
    g_track_count = 0;
}

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (unsigned char)s[start] <= 0x20) start++;
    size_t end = s.size();
    while (end > start && (unsigned char)s[end - 1] <= 0x20) end--;
    return s.substr(start, end - start);
}

static bool starts_with(const std::string& s, const char* prefix) {
    size_t len = strlen(prefix);
    return s.size() >= len && s.compare(0, len, prefix) == 0;
}

static bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string to_lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static std::string read_text_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return ""; }
    std::string buf(sz, '\0');
    size_t read_n = fread(&buf[0], 1, sz, f);
    fclose(f);
    buf.resize(read_n);
    return buf;
}

static std::vector<uint8_t> read_binary_file(const char* path) {
    std::vector<uint8_t> result;
    FILE* f = fopen(path, "rb");
    if (!f) return result;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return result; }
    result.resize(sz);
    size_t read_n = fread(result.data(), 1, sz, f);
    fclose(f);
    result.resize(read_n);
    return result;
}

static void add_event(int track, const Event& evt) {
    g_events[track].push_back(evt);
}

static void parse_rpp(const std::string& content) {
    g_track_count = 0;
    int current_track = 0;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t line_end = content.find('\n', pos);
        if (line_end == std::string::npos) line_end = content.size();
        std::string line = content.substr(pos, line_end - pos);
        pos = line_end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(line);
        if (line.empty()) continue;

        if (starts_with(line, "<TRACK")) {
            current_track++;
            if (current_track > g_track_count) g_track_count = current_track;
        } else if (starts_with(line, "<ITEM") && current_track >= 1) {
            Event evt = {};
            evt.track = current_track;
            evt.event_type = 0;
            evt.velocity = 0.0;
            int depth = 1;
            while (pos < content.size()) {
                size_t next_end = content.find('\n', pos);
                if (next_end == std::string::npos) next_end = content.size();
                std::string inner = content.substr(pos, next_end - pos);
                pos = next_end + 1;
                if (!inner.empty() && inner.back() == '\r') inner.pop_back();
                inner = trim(inner);
                if (starts_with(inner, "<")) { depth++; }
                if (inner == ">") { depth--; if (depth <= 0) break; continue; }
                if (starts_with(inner, "POSITION")) {
                    const char* p = inner.c_str() + 8;
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p) evt.position_sec = atof(p);
                } else if (starts_with(inner, "LENGTH")) {
                    const char* p = inner.c_str() + 6;
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p) evt.length_sec = atof(p);
                } else if (starts_with(inner, "PLAYRATE")) {
                    const char* p = inner.c_str() + 8;
                    while (*p == ' ' || *p == '\t') p++;
                    std::string remaining(p);
                    size_t sp1 = remaining.find(' ');
                    if (sp1 == std::string::npos) continue;
                    std::string token2 = trim(remaining.substr(sp1 + 1));
                    size_t sp2 = token2.find(' ');
                    if (sp2 == std::string::npos) continue;
                    std::string token3 = trim(token2.substr(sp2 + 1));
                    if (!token3.empty()) evt.pitch_shift = atof(token3.c_str());
                }
            }
            add_event(current_track, evt);
        }
    }
}

static uint16_t read_be16(const uint8_t*& p) {
    uint16_t v = ((uint16_t)p[0] << 8) | p[1]; p += 2; return v;
}
static int16_t read_be16s(const uint8_t*& p) {
    int16_t v = (int16_t)(((uint16_t)p[0] << 8) | p[1]); p += 2; return v;
}
static uint32_t read_be32(const uint8_t*& p) {
    uint32_t v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; p += 4; return v;
}
static uint32_t read_varlen(const uint8_t*& p, const uint8_t* end) {
    uint32_t v = 0;
    while (p < end) { uint8_t b = *p++; v = (v << 7) | (b & 0x7F); if ((b & 0x80) == 0) break; }
    return v;
}

struct PendingNote { uint8_t note; uint8_t channel; double start_sec; };

static void parse_midi(const std::vector<uint8_t>& data) {
    g_track_count = 0;
    const uint8_t* p = data.data();
    const uint8_t* end = p + data.size();
    if (end - p < 8 || memcmp(p, "MThd", 4) != 0) return;
    p += 4;
    uint32_t header_len = read_be32(p);
    if (header_len < 6) return;
    /*uint16_t format =*/ read_be16(p);
    uint16_t ntrks = read_be16(p);
    int16_t division = read_be16s(p);
    p += header_len - 6;
    if (division < 0) return;

    double ticks_per_beat = (double)division;
    double usec_per_beat = 500000.0;
    double usec_per_tick = usec_per_beat / ticks_per_beat;

    for (int track_idx = 0; track_idx < (int)ntrks; track_idx++) {
        if (end - p < 8 || memcmp(p, "MTrk", 4) != 0) break;
        p += 4;
        uint32_t track_len = read_be32(p);
        const uint8_t* track_start = p;
        const uint8_t* track_end = p + track_len;
        std::vector<PendingNote> pending;
        double abs_time_sec = 0.0;
        uint8_t running_status = 0;

        while (p < track_end) {
            uint32_t delta = read_varlen(p, track_end);
            if (p >= track_end) break;
            abs_time_sec += delta * usec_per_tick / 1000000.0;
            uint8_t byte = *p;
            uint8_t status;
            if (byte < 0x80) {
                if (running_status == 0) { p++; continue; }
                status = running_status;
            } else {
                status = *p++;
                if (status < 0xF0) running_status = status; else running_status = 0;
            }

            if (status == 0xFF) {
                if (p >= track_end) break;
                uint8_t meta_type = *p++;
                uint32_t meta_len = read_varlen(p, track_end);
                const uint8_t* meta_data = p;
                p += meta_len;
                if (meta_type == 0x51 && meta_len == 3 && meta_data + 3 <= track_end) {
                    usec_per_beat = (double)(((uint32_t)meta_data[0] << 16) | ((uint32_t)meta_data[1] << 8) | meta_data[2]);
                    usec_per_tick = usec_per_beat / ticks_per_beat;
                }
                if (meta_type == 0x2F) break;
            } else if (status == 0xF0 || status == 0xF7) {
                uint32_t sysex_len = read_varlen(p, track_end);
                p += sysex_len;
            } else if (status < 0xF0) {
                uint8_t msg_type = (status >> 4) & 0x0F;
                uint8_t channel = status & 0x0F;
                if (msg_type == 0x8 || msg_type == 0x9) {
                    if (p + 2 > track_end) break;
                    uint8_t note = *p++;
                    uint8_t velocity = *p++;
                    if (msg_type == 0x9 && velocity > 0) {
                        PendingNote pn; pn.note = note; pn.channel = channel; pn.start_sec = abs_time_sec;
                        pending.push_back(pn);
                    } else {
                        for (size_t i = 0; i < pending.size(); i++) {
                            if (pending[i].note == note && pending[i].channel == channel) {
                                Event evt = {};
                                evt.position_sec = pending[i].start_sec;
                                evt.length_sec = abs_time_sec - pending[i].start_sec;
                                evt.pitch_shift = (double)((int)note - 60);
                                evt.velocity = (double)velocity;
                                evt.track = track_idx + 1;
                                evt.event_type = 1;
                                int store_track = 100 + track_idx;
                                add_event(store_track, evt);
                                pending.erase(pending.begin() + i);
                                break;
                            }
                        }
                    }
                } else if (msg_type == 0xA) { if (p + 2 <= track_end) p += 2; else break; }
                else if (msg_type == 0xB) { if (p + 2 <= track_end) p += 2; else break; }
                else if (msg_type == 0xC) { if (p + 1 <= track_end) p += 1; else break; }
                else if (msg_type == 0xD) { if (p + 1 <= track_end) p += 1; else break; }
                else if (msg_type == 0xE) { if (p + 2 <= track_end) p += 2; else break; }
            }
        }
        for (auto& pn : pending) {
            Event evt = {};
            evt.position_sec = pn.start_sec;
            evt.length_sec = abs_time_sec - pn.start_sec;
            evt.pitch_shift = (double)((int)pn.note - 60);
            evt.velocity = 0.0;
            evt.track = track_idx + 1;
            evt.event_type = 1;
            int store_track = 100 + track_idx;
            add_event(store_track, evt);
        }
        p = track_start + track_len;
        if (track_idx != 0) {
            if (100 + track_idx > g_track_count) g_track_count = 100 + track_idx;
        }
    }
}

static void compute_track_meta() {
    g_track_meta.clear();
    for (auto& kv : g_events) {
        int track = kv.first;
        auto& events = kv.second;
        if (events.empty()) continue;
        TrackMeta meta;
        meta.item_count = (int)events.size();
        meta.min_pitch = events[0].pitch_shift;
        meta.max_pitch = events[0].pitch_shift;
        for (auto& e : events) {
            if (e.pitch_shift < meta.min_pitch) meta.min_pitch = e.pitch_shift;
            if (e.pitch_shift > meta.max_pitch) meta.max_pitch = e.pitch_shift;
        }
        if (meta.min_pitch == meta.max_pitch) { meta.min_pitch -= 1.0; meta.max_pitch += 1.0; }
        g_track_meta[track] = meta;
    }
}

// Expression evaluator
enum class ExprTokenType {
    NUMBER, PLUS, MINUS, STAR, SLASH, PERCENT, CARET,
    LPAREN, RPAREN, COMMA, IDENTIFIER, END
};

struct Token {
    ExprTokenType type;
    double num_value;
    std::string id_value;
    Token() : type(ExprTokenType::END), num_value(0.0) {}
    explicit Token(ExprTokenType t) : type(t), num_value(0.0) {}
    Token(ExprTokenType t, double v) : type(t), num_value(v) {}
    Token(ExprTokenType t, const std::string& id) : type(t), num_value(0.0), id_value(id) {}
};

class ExprParser {
public:
    explicit ExprParser(const std::string& str) : src_(str) {}

    bool parse(std::string& error_msg) {
        tokenize();
        if (tokens_.empty()) { error_msg = "empty expression"; return false; }
        tok_idx_ = 0;
        double r = expr(error_msg);
        if (!error_msg.empty()) return false;
        if (tok_idx_ < tokens_.size() && tokens_[tok_idx_].type != ExprTokenType::END) {
            error_msg = "unexpected trailing tokens"; return false;
        }
        result_ = r;
        return true;
    }
    double get_result() const { return result_; }

private:
    const std::string& src_;
    std::vector<Token> tokens_;
    size_t tok_idx_ = 0;
    double result_ = 0.0;

    void tokenize() {
        size_t i = 0;
        while (i < src_.size()) {
            char c = src_[i];
            if (c <= ' ') { i++; continue; }
            if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < src_.size() && std::isdigit((unsigned char)src_[i + 1]))) {
                size_t start = i; bool has_dot = false, has_exp = false;
                while (i < src_.size()) {
                    char nc = src_[i];
                    if (std::isdigit((unsigned char)nc)) { i++; }
                    else if (nc == '.' && !has_dot && !has_exp) { has_dot = true; i++; }
                    else if ((nc == 'e' || nc == 'E') && !has_exp) { has_exp = true; i++; if (i < src_.size() && (src_[i] == '+' || src_[i] == '-')) i++; }
                    else break;
                }
                tokens_.push_back(Token(ExprTokenType::NUMBER, atof(src_.substr(start, i - start).c_str())));
                continue;
            }
            if (std::isalpha((unsigned char)c) || c == '_') {
                size_t start = i;
                while (i < src_.size() && (std::isalnum((unsigned char)src_[i]) || src_[i] == '_' || src_[i] == '.')) i++;
                tokens_.push_back(Token(ExprTokenType::IDENTIFIER, src_.substr(start, i - start)));
                continue;
            }
            switch (c) {
                case '+': tokens_.push_back(Token(ExprTokenType::PLUS)); i++; break;
                case '-': tokens_.push_back(Token(ExprTokenType::MINUS)); i++; break;
                case '*': tokens_.push_back(Token(ExprTokenType::STAR)); i++; break;
                case '/': tokens_.push_back(Token(ExprTokenType::SLASH)); i++; break;
                case '%': tokens_.push_back(Token(ExprTokenType::PERCENT)); i++; break;
                case '^': tokens_.push_back(Token(ExprTokenType::CARET)); i++; break;
                case '(': tokens_.push_back(Token(ExprTokenType::LPAREN)); i++; break;
                case ')': tokens_.push_back(Token(ExprTokenType::RPAREN)); i++; break;
                case ',': tokens_.push_back(Token(ExprTokenType::COMMA)); i++; break;
                default: i++; break;
            }
        }
        tokens_.push_back(Token(ExprTokenType::END));
    }

    Token& peek() { return tokens_[tok_idx_]; }
    Token& advance() { return tokens_[tok_idx_++]; }

    double expr(std::string& err) {
        double left = term(err);
        if (!err.empty()) return 0;
        while (peek().type == ExprTokenType::PLUS || peek().type == ExprTokenType::MINUS) {
            ExprTokenType op = advance().type;
            double right = term(err);
            if (!err.empty()) return 0;
            left = (op == ExprTokenType::PLUS) ? left + right : left - right;
        }
        return left;
    }
    double term(std::string& err) {
        double left = factor(err);
        if (!err.empty()) return 0;
        while (peek().type == ExprTokenType::STAR || peek().type == ExprTokenType::SLASH || peek().type == ExprTokenType::PERCENT) {
            ExprTokenType op = advance().type;
            double right = factor(err);
            if (!err.empty()) return 0;
            if (op == ExprTokenType::STAR) left *= right;
            else if (op == ExprTokenType::SLASH) { if (right == 0.0) { err = "division by zero"; return 0; } left /= right; }
            else { if (right == 0.0) { err = "modulo by zero"; return 0; } left = fmod(left, right); }
        }
        return left;
    }
    double factor(std::string& err) {
        double left = unary(err);
        if (!err.empty()) return 0;
        if (peek().type == ExprTokenType::CARET) {
            advance(); double right = factor(err);
            if (!err.empty()) return 0;
            left = pow(left, right);
        }
        return left;
    }
    double unary(std::string& err) {
        if (peek().type == ExprTokenType::MINUS) { advance(); return -unary(err); }
        if (peek().type == ExprTokenType::PLUS) { advance(); return unary(err); }
        return primary(err);
    }
    double primary(std::string& err) {
        Token& t = peek();
        if (t.type == ExprTokenType::NUMBER) { advance(); return t.num_value; }
        if (t.type == ExprTokenType::LPAREN) {
            advance(); double val = expr(err);
            if (err.empty() && peek().type == ExprTokenType::RPAREN) { advance(); return val; }
            if (err.empty()) err = "missing ')'";
            return 0;
        }
        if (t.type == ExprTokenType::IDENTIFIER) {
            std::string id = t.id_value; advance();
            if (peek().type == ExprTokenType::LPAREN) {
                advance();
                std::vector<double> args;
                if (peek().type != ExprTokenType::RPAREN) {
                    while (true) {
                        double arg = expr(err); if (!err.empty()) return 0;
                        args.push_back(arg);
                        if (peek().type == ExprTokenType::COMMA) advance(); else break;
                    }
                }
                if (peek().type != ExprTokenType::RPAREN) { err = "missing ')' after function args"; return 0; }
                advance();
                return call_func(id, args, err);
            }
            err = "unknown variable: " + id; return 0;
        }
        err = "unexpected token"; return 0;
    }

    double call_func(const std::string& name, const std::vector<double>& args, std::string& err) {
        size_t n = args.size();
        if (name == "sin")   { if(n!=1){err="sin() takes 1 arg";return 0;} return sin(args[0]); }
        if (name == "cos")   { if(n!=1){err="cos() takes 1 arg";return 0;} return cos(args[0]); }
        if (name == "tan")   { if(n!=1){err="tan() takes 1 arg";return 0;} return tan(args[0]); }
        if (name == "atan")  { if(n!=1){err="atan() takes 1 arg";return 0;} return atan(args[0]); }
        if (name == "sqrt")  { if(n!=1){err="sqrt() takes 1 arg";return 0;} if(args[0]<0){err="sqrt of negative";return 0;} return sqrt(args[0]); }
        if (name == "abs")   { if(n!=1){err="abs() takes 1 arg";return 0;} return fabs(args[0]); }
        if (name == "floor") { if(n!=1){err="floor() takes 1 arg";return 0;} return floor(args[0]); }
        if (name == "ceil")  { if(n!=1){err="ceil() takes 1 arg";return 0;} return ceil(args[0]); }
        if (name == "log")   { if(n!=1){err="log() takes 1 arg";return 0;} if(args[0]<=0){err="log of non-positive";return 0;} return log(args[0]); }
        if (name == "exp")   { if(n!=1){err="exp() takes 1 arg";return 0;} return exp(args[0]); }
        if (name == "pow")   { if(n!=2){err="pow() takes 2 args";return 0;} return pow(args[0], args[1]); }
        if (name == "min") {
            if (n < 2) { err = "min() needs 2+ args"; return 0; }
            double m = args[0]; for (size_t i = 1; i < n; i++) if (args[i] < m) m = args[i]; return m;
        }
        if (name == "max") {
            if (n < 2) { err = "max() needs 2+ args"; return 0; }
            double m = args[0]; for (size_t i = 1; i < n; i++) if (args[i] > m) m = args[i]; return m;
        }
        if (name == "pi") { if (n != 0) { err = "pi takes no args"; return 0; } return 3.14159265358979323846; }
        err = "unknown function: " + name; return 0;
    }
};

static std::string replace_var(const std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    std::string result = s;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        bool left_ok = (pos == 0) || (!std::isalnum((unsigned char)result[pos - 1]) && result[pos - 1] != '_');
        bool right_ok = (pos + from.size() >= result.size()) ||
                        (!std::isalnum((unsigned char)result[pos + from.size()]) && result[pos + from.size()] != '_');
        if (left_ok && right_ok) { result.replace(pos, from.size(), to); pos += to.size(); }
        else { pos += from.size(); }
    }
    return result;
}

static std::string preprocess_expr(const std::string& expr, double pitch, double velocity, double t, double item_idx, double track) {
    std::string s = expr;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.15g", pitch);    s = replace_var(s, "pitch", buf);
    snprintf(buf, sizeof(buf), "%.15g", velocity); s = replace_var(s, "velocity", buf);
    snprintf(buf, sizeof(buf), "%.15g", t);        s = replace_var(s, "time", buf);
    snprintf(buf, sizeof(buf), "%.15g", item_idx); s = replace_var(s, "item_idx", buf);
    snprintf(buf, sizeof(buf), "%.15g", track);    s = replace_var(s, "track", buf);
    return s;
}

// Module functions
static void parse_file(SCRIPT_MODULE_PARAM* param) {
    clear_state();
    LPCSTR path = param->get_param_string(0);
    if (!path || path[0] == '\0') { param->set_error("parse_file: empty file path"); return; }
    std::string path_str(path);
    std::string lower = to_lower(path_str);
    if (ends_with(lower, ".rpp")) {
        std::string content = read_text_file(path);
        if (content.empty()) { param->set_error("parse_file: failed to read RPP file or file is empty"); return; }
        parse_rpp(content);
    } else if (ends_with(lower, ".mid") || ends_with(lower, ".midi") || ends_with(lower, ".smf")) {
        std::vector<uint8_t> data = read_binary_file(path);
        if (data.empty()) { param->set_error("parse_file: failed to read MIDI file or file is empty"); return; }
        parse_midi(data);
    } else {
        param->set_error("parse_file: unsupported file format (use .rpp, .mid, .midi, or .smf)");
        return;
    }
    compute_track_meta();
}

static void get_track_count(SCRIPT_MODULE_PARAM* param) {
    param->push_result_int(g_track_count);
}

static void get_event_count(SCRIPT_MODULE_PARAM* param) {
    int track = param->get_param_int(0);
    auto it = g_events.find(track);
    param->push_result_int(it != g_events.end() ? (int)it->second.size() : 0);
}

static void get_event_data(SCRIPT_MODULE_PARAM* param) {
    int track = param->get_param_int(0);
    auto it = g_events.find(track);
    if (it == g_events.end() || it->second.empty()) {
        double dummy = 0.0; param->push_result_array_double(&dummy, 0); return;
    }
    auto& events = it->second;
    std::vector<double> flat; flat.reserve(events.size() * 4);
    for (auto& e : events) {
        flat.push_back(e.position_sec);
        flat.push_back(e.length_sec);
        flat.push_back(e.pitch_shift);
        flat.push_back(e.velocity);
    }
    param->push_result_array_double(flat.data(), (int)flat.size());
}

static void get_track_meta(SCRIPT_MODULE_PARAM* param) {
    int track = param->get_param_int(0);
    auto it = g_track_meta.find(track);
    LPCSTR keys[3] = { "min_pitch", "max_pitch", "item_count" };
    double values[3];
    if (it != g_track_meta.end()) {
        values[0] = it->second.min_pitch;
        values[1] = it->second.max_pitch;
        values[2] = (double)it->second.item_count;
    } else {
        values[0] = -12.0; values[1] = 12.0; values[2] = 0.0;
    }
    param->push_result_table_double(keys, values, 3);
}

static void eval_expression(SCRIPT_MODULE_PARAM* param) {
    LPCSTR expr = param->get_param_string(0);
    double pitch = param->get_param_double(1);
    double velocity = param->get_param_double(2);
    double t = param->get_param_double(3);
    double item_idx = param->get_param_double(4);
    double track = param->get_param_double(5);
    if (!expr || expr[0] == '\0') { param->set_error("eval_expression: empty expression"); return; }
    std::string processed = preprocess_expr(expr, pitch, velocity, t, item_idx, track);
    ExprParser parser(processed);
    std::string error;
    if (!parser.parse(error)) { param->set_error(error.c_str()); return; }
    param->push_result_double(parser.get_result());
}

// Module table and exports
static SCRIPT_MODULE_FUNCTION g_functions[] = {
    { L"parse_file", parse_file },
    { L"get_track_count", get_track_count },
    { L"get_event_count", get_event_count },
    { L"get_event_data", get_event_data },
    { L"get_track_meta", get_track_meta },
    { L"eval_expression", eval_expression },
    { nullptr, nullptr }
};

static SCRIPT_MODULE_TABLE g_script_module_table = {
    L"AutoZYC Parse Module v2.00",
    g_functions
};

extern "C" {
__declspec(dllexport) SCRIPT_MODULE_TABLE* GetScriptModuleTable(void) { return &g_script_module_table; }
__declspec(dllexport) bool InitializePlugin(DWORD version) { (void)version; return true; }
__declspec(dllexport) void UninitializePlugin() { clear_state(); }
}