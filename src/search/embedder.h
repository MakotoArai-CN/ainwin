// Local embeddings through ONNX Runtime, no service and no network.
//
// The model is a BERT-family encoder with a WordPiece vocabulary, quantized
// to INT8 (bge-small-zh-v1.5 by default: 24M parameters, 512 dimensions).
// onnxruntime.dll is loaded from the plugin folder at run time, so the exe
// has no import dependency on it and a machine without the model still gets
// the keyword search.
#pragma once

#include "common.h"

#include "onnxruntime_c_api.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace finder {

class Embedder {
public:
    Embedder() = default;
    ~Embedder() { unload(); }
    Embedder(const Embedder&) = delete;
    Embedder& operator=(const Embedder&) = delete;

    bool ready() const { return m_session != nullptr; }
    const std::string& error() const { return m_error; }
    const std::string& modelName() const { return m_modelName; }
    int dimension() const { return m_dimension; }
    void reset()
    {
        unload();
        m_error.clear();
        m_modelName.clear();
    }

    // `dir`: the folder holding model_quantized.onnx (or model.onnx),
    // vocab.txt and onnxruntime.dll.
    bool load(const std::string& dir, int threads, int maxLength)
    {
        unload();
        m_dir = dir;
        m_maxLength = std::clamp(maxLength, 16, 512);
        const std::wstring base = widen(dir);
        if (base.empty()) {
            m_error = "empty model folder";
            return false;
        }

        std::wstring dll = base + L"\\onnxruntime.dll";
        m_ort = ::LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!m_ort) {
            // The DLL usually sits at the plugin root, the model one or two
            // folders below it.
            for (const wchar_t* relative : {L"\\..\\onnxruntime.dll", L"\\..\\..\\onnxruntime.dll"}) {
                dll = base + relative;
                m_ort = ::LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
                if (m_ort)
                    break;
            }
        }
        if (!m_ort) {
            m_ort = ::LoadLibraryW(L"onnxruntime.dll");
            if (!m_ort) {
                m_error = "onnxruntime.dll not found next to the model";
                return false;
            }
        }
        using GetBase = const OrtApiBase*(ORT_API_CALL*)();
        const auto getBase = reinterpret_cast<GetBase>(reinterpret_cast<void*>(::GetProcAddress(m_ort, "OrtGetApiBase")));
        if (!getBase) {
            wchar_t loaded[32768];
            const DWORD n = ::GetModuleFileNameW(m_ort, loaded, 32768);
            m_error = "onnxruntime.dll has no OrtGetApiBase";
            if (n > 0)
                m_error += " (" + utf8Of(std::wstring(loaded, n)) + ")";
            unload();
            return false;
        }
        const OrtApiBase* apiBase = getBase();
        m_api = apiBase ? apiBase->GetApi(ORT_API_VERSION) : nullptr;
        if (!m_api) {
            m_error = std::string("onnxruntime is too old (needs API ") + std::to_string(ORT_API_VERSION) + ")";
            unload();
            return false;
        }

        if (!check(m_api->CreateEnv(ORT_LOGGING_LEVEL_ERROR, "ainwin", &m_env), "CreateEnv"))
            return false;

        std::wstring modelPath;
        for (const wchar_t* name : {L"\\model.onnx", L"\\model_quantized.onnx", L"\\model_int8.onnx", L"\\model_uint8.onnx"}) {
            const std::wstring candidate = base + name;
            if (::GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
                modelPath = candidate;
                m_modelName = utf8Of(std::wstring(name).substr(1));
                break;
            }
        }
        if (modelPath.empty()) {
            m_error = "no model.onnx in " + dir;
            unload();
            return false;
        }
        if (!loadVocab(base + L"\\vocab.txt")) {
            m_error = "vocab.txt is missing or empty";
            unload();
            return false;
        }

        OrtSessionOptions* options = nullptr;
        if (!check(m_api->CreateSessionOptions(&options), "CreateSessionOptions"))
            return false;
        m_options = options;
        check(m_api->SetIntraOpNumThreads(options, std::clamp(threads, 1, 8)), "SetIntraOpNumThreads");
        check(m_api->SetSessionGraphOptimizationLevel(options, ORT_ENABLE_ALL), "SetGraphOptimization");

        if (!check(m_api->CreateSession(m_env, modelPath.c_str(), options, &m_session), "CreateSession")) {
            unload();
            return false;
        }
        if (!check(m_api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &m_memory), "CreateCpuMemoryInfo")) {
            unload();
            return false;
        }
        if (!describeInputs()) {
            unload();
            return false;
        }
        m_loaded = true;
        return true;
    }

    // One vector per text, L2-normalised. `texts` may be large; they are
    // batched and bucketed by length so padding is not wasted.
    bool embed(const std::vector<std::string>& texts, bool query, std::vector<std::vector<float>>& out, std::string& error)
    {
        out.clear();
        if (!ready()) {
            error = m_error.empty() ? "model is not loaded" : m_error;
            return false;
        }
        const int maxLen = query ? std::min(m_maxLength, 96) : m_maxLength;
        std::vector<std::vector<int64_t>> encoded;
        encoded.reserve(texts.size());
        for (const std::string& text : texts)
            encoded.push_back(encode(query ? std::string(kQueryPrefix) + text : text, maxLen));

        out.resize(texts.size());
        std::vector<std::size_t> order(texts.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return encoded[a].size() < encoded[b].size(); });

        constexpr std::size_t kBatch = 32;
        for (std::size_t at = 0; at < order.size(); at += kBatch) {
            const std::size_t count = std::min(kBatch, order.size() - at);
            std::size_t seq = 1;
            for (std::size_t i = 0; i < count; ++i)
                seq = std::max(seq, encoded[order[at + i]].size());
            std::vector<int64_t> ids(count * seq, m_padId);
            std::vector<int64_t> mask(count * seq, 0);
            std::vector<int64_t> types(count * seq, 0);
            for (std::size_t i = 0; i < count; ++i) {
                const std::vector<int64_t>& row = encoded[order[at + i]];
                std::copy(row.begin(), row.end(), ids.begin() + static_cast<std::ptrdiff_t>(i * seq));
                std::fill(mask.begin() + static_cast<std::ptrdiff_t>(i * seq), mask.begin() + static_cast<std::ptrdiff_t>(i * seq + row.size()), 1);
            }
            std::vector<std::vector<float>> vectors;
            if (!runBatch(ids, mask, types, count, seq, vectors, error))
                return false;
            for (std::size_t i = 0; i < count; ++i)
                out[order[at + i]] = std::move(vectors[i]);
        }
        return true;
    }

    static float cosine(const std::vector<float>& a, const std::vector<float>& b)
    {
        if (a.size() != b.size() || a.empty())
            return 0;
        float dot = 0;
        for (std::size_t i = 0; i < a.size(); ++i)
            dot += a[i] * b[i];
        return dot;
    }

private:
    static constexpr const char* kQueryPrefix = "为这个句子生成表示以用于检索相关文章：";

    static std::wstring widen(const std::string& s)
    {
        if (s.empty())
            return {};
        const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
        return w;
    }

    bool check(OrtStatus* status, const char* what)
    {
        if (!status)
            return true;
        if (m_api) {
            const char* message = m_api->GetErrorMessage(status);
            m_error = std::string(what) + ": " + (message ? message : "unknown error");
            m_api->ReleaseStatus(status);
        }
        return false;
    }

    void unload()
    {
        if (m_api) {
            if (m_memory) {
                m_api->ReleaseMemoryInfo(m_memory);
                m_memory = nullptr;
            }
            if (m_session) {
                m_api->ReleaseSession(m_session);
                m_session = nullptr;
            }
            if (m_options) {
                m_api->ReleaseSessionOptions(m_options);
                m_options = nullptr;
            }
            if (m_env) {
                m_api->ReleaseEnv(m_env);
                m_env = nullptr;
            }
        }
        if (m_ort) {
            ::FreeLibrary(m_ort);
            m_ort = nullptr;
        }
        m_api = nullptr;
        m_loaded = false;
    }

    bool loadVocab(const std::wstring& path)
    {
        FILE* file = _wfopen(path.c_str(), L"rb");
        if (!file)
            return false;
        std::string token;
        int id = 0;
        char buffer[8192];
        while (std::fgets(buffer, sizeof buffer, file)) {
            token = buffer;
            while (!token.empty() && (token.back() == '\n' || token.back() == '\r'))
                token.pop_back();
            if (id == 0 && token.size() >= 3 && static_cast<unsigned char>(token[0]) == 0xEF)
                token = token.substr(3); // UTF-8 BOM
            if (!token.empty())
                m_vocab[token] = id;
            ++id;
        }
        std::fclose(file);
        auto find = [&](const char* name) {
            const auto it = m_vocab.find(name);
            return it == m_vocab.end() ? -1 : it->second;
        };
        m_clsId = find("[CLS]");
        m_sepId = find("[SEP]");
        m_unkId = find("[UNK]");
        m_padId = find("[PAD]");
        if (m_padId < 0)
            m_padId = 0;
        return m_clsId >= 0 && m_sepId >= 0 && m_unkId >= 0 && m_vocab.size() > 100;
    }

    bool describeInputs()
    {
        OrtAllocator* allocator = nullptr;
        if (!check(m_api->GetAllocatorWithDefaultOptions(&allocator), "GetAllocator"))
            return false;
        std::size_t count = 0;
        if (!check(m_api->SessionGetInputCount(m_session, &count), "InputCount"))
            return false;
        for (std::size_t i = 0; i < count; ++i) {
            char* name = nullptr;
            if (!check(m_api->SessionGetInputName(m_session, i, allocator, &name), "InputName"))
                return false;
            m_inputNames.emplace_back(name ? name : "");
            if (name)
                allocator->Free(allocator, name);
        }
        std::size_t outputs = 0;
        if (!check(m_api->SessionGetOutputCount(m_session, &outputs), "OutputCount") || outputs == 0)
            return false;
        char* output = nullptr;
        if (!check(m_api->SessionGetOutputName(m_session, 0, allocator, &output), "OutputName"))
            return false;
        m_outputName = output ? output : "";
        if (output)
            allocator->Free(allocator, output);
        for (const std::string& name : m_inputNames) {
            if (name.find("input_ids") == std::string::npos && name.find("attention_mask") == std::string::npos
                && name.find("token_type_ids") == std::string::npos) {
                m_error = "unexpected model input: " + name;
                return false;
            }
        }
        return !m_inputNames.empty() && !m_outputName.empty();
    }

    // ---- tokenizer (BERT WordPiece) ---------------------------------------------

    struct Unit {
        bool word;
        std::string text;
    };

    static bool isCjk(unsigned cp)
    {
        return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0xF900 && cp <= 0xFAFF)
            || (cp >= 0x3040 && cp <= 0x30FF) || (cp >= 0xAC00 && cp <= 0xD7AF);
    }

    static bool isPunct(unsigned cp)
    {
        if (cp < 0x80)
            return std::ispunct(static_cast<int>(cp)) != 0;
        return (cp >= 0x3000 && cp <= 0x303F) || (cp >= 0xFF00 && cp <= 0xFFEF) || cp == 0x2018 || cp == 0x2019
            || cp == 0x201C || cp == 0x201D || cp == 0x2026 || cp == 0x2014;
    }

    static std::vector<std::pair<unsigned, std::string>> codePoints(const std::string& s)
    {
        std::vector<std::pair<unsigned, std::string>> out;
        for (std::size_t i = 0; i < s.size();) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            unsigned cp = c;
            std::size_t n = 1;
            if (c >= 0xF0) {
                cp = c & 0x07;
                n = 4;
            } else if (c >= 0xE0) {
                cp = c & 0x0F;
                n = 3;
            } else if (c >= 0xC0) {
                cp = c & 0x1F;
                n = 2;
            }
            if (i + n > s.size())
                n = 1;
            for (std::size_t k = 1; k < n; ++k)
                cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
            out.emplace_back(cp, s.substr(i, n));
            i += n;
        }
        return out;
    }

    std::vector<Unit> basicUnits(const std::string& text) const
    {
        std::vector<Unit> units;
        std::string word;
        for (const auto& [cp, bytes] : codePoints(text)) {
            if (cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n' || cp == 0x3000) {
                if (!word.empty()) {
                    units.push_back({true, word});
                    word.clear();
                }
            } else if (isCjk(cp) || isPunct(cp)) {
                if (!word.empty()) {
                    units.push_back({true, word});
                    word.clear();
                }
                units.push_back({false, bytes});
            } else if (cp < 0x20) {
                continue;
            } else {
                word += bytes;
            }
        }
        if (!word.empty())
            units.push_back({true, word});
        return units;
    }

    std::vector<int64_t> wordPiece(const std::string& word) const
    {
        std::vector<int64_t> ids;
        const auto chars = codePoints(word);
        if (chars.size() > 100) {
            ids.push_back(m_unkId);
            return ids;
        }
        std::size_t start = 0;
        while (start < chars.size()) {
            bool found = false;
            for (std::size_t end = chars.size(); end > start; --end) {
                std::string piece;
                if (start > 0)
                    piece = "##";
                for (std::size_t i = start; i < end; ++i)
                    piece += chars[i].second;
                auto it = m_vocab.find(piece);
                if (it == m_vocab.end() && start == 0 && !piece.empty()) {
                    const std::string lower = lowerAscii(piece);
                    it = m_vocab.find(lower);
                }
                if (it != m_vocab.end()) {
                    ids.push_back(it->second);
                    start = end;
                    found = true;
                    break;
                }
            }
            if (!found) {
                ids.push_back(m_unkId);
                break;
            }
        }
        return ids;
    }

    std::vector<int64_t> encode(const std::string& text, int maxLen) const
    {
        std::vector<int64_t> ids;
        ids.push_back(m_clsId);
        for (const Unit& unit : basicUnits(text)) {
            if (static_cast<int>(ids.size()) >= maxLen - 1)
                break;
            if (!unit.word) {
                auto it = m_vocab.find(unit.text);
                ids.push_back(it == m_vocab.end() ? m_unkId : it->second);
                continue;
            }
            for (int64_t id : wordPiece(unit.text)) {
                if (static_cast<int>(ids.size()) >= maxLen - 1)
                    break;
                ids.push_back(id);
            }
        }
        ids.push_back(m_sepId);
        return ids;
    }

    bool runBatch(const std::vector<int64_t>& ids, const std::vector<int64_t>& mask, const std::vector<int64_t>& types,
        std::size_t batch, std::size_t seq, std::vector<std::vector<float>>& out, std::string& error)
    {
        const int64_t dims[2] = {static_cast<int64_t>(batch), static_cast<int64_t>(seq)};
        std::vector<OrtValue*> values(m_inputNames.size(), nullptr);
        std::vector<OrtValue*> created;
        auto releaseInputs = [&] {
            for (OrtValue* value : created) {
                if (value)
                    m_api->ReleaseValue(value);
            }
            created.clear();
            values.assign(m_inputNames.size(), nullptr);
        };
        for (std::size_t i = 0; i < m_inputNames.size(); ++i) {
            const std::string& name = m_inputNames[i];
            const int64_t* data = nullptr;
            void* mutableData = nullptr;
            if (name.find("input_ids") != std::string::npos)
                data = ids.data();
            else if (name.find("attention_mask") != std::string::npos)
                data = mask.data();
            else if (name.find("token_type_ids") != std::string::npos)
                data = types.data();
            else
                continue;
            mutableData = const_cast<int64_t*>(data);
            OrtValue* value = nullptr;
            if (!check(m_api->CreateTensorWithDataAsOrtValue(m_memory, mutableData,
                           ids.size() * sizeof(int64_t), dims, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &value),
                    "CreateTensor")) {
                releaseInputs();
                error = m_error;
                return false;
            }
            created.push_back(value);
            values[i] = value;
        }

        const char* inputNames[8];
        for (std::size_t i = 0; i < m_inputNames.size(); ++i)
            inputNames[i] = m_inputNames[i].c_str();
        const char* outputNames[1] = {m_outputName.c_str()};
        OrtValue* output = nullptr;
        OrtStatus* status = m_api->Run(m_session, nullptr, inputNames, values.data(), values.size(), outputNames, 1, &output);
        releaseInputs();
        if (status) {
            const char* message = m_api->GetErrorMessage(status);
            error = std::string("Run: ") + (message ? message : "failed");
            m_api->ReleaseStatus(status);
            if (output)
                m_api->ReleaseValue(output);
            return false;
        }

        OrtTensorTypeAndShapeInfo* info = nullptr;
        bool ok = true;
        if (check(m_api->GetTensorTypeAndShape(output, &info), "TensorShape")) {
            std::size_t rank = 0;
            m_api->GetDimensionsCount(info, &rank);
            std::vector<int64_t> shape(rank, 0);
            if (rank >= 2)
                m_api->GetDimensions(info, shape.data(), rank);
            void* raw = nullptr;
            m_api->GetTensorMutableData(output, &raw);
            const auto* data = static_cast<const float*>(raw);
            const int64_t hidden = shape.empty() ? 0 : shape.back();
            if (rank < 2 || hidden <= 0 || data == nullptr) {
                error = "unexpected model output shape";
                ok = false;
            } else {
                out.resize(batch);
                for (std::size_t b = 0; b < batch; ++b) {
                    const float* row = rank >= 3 ? data + (b * seq) * static_cast<std::size_t>(hidden) : data + b * static_cast<std::size_t>(hidden);
                    std::vector<float> vector(row, row + hidden);
                    float norm = 0;
                    for (float value : vector)
                        norm += value * value;
                    norm = std::sqrt(norm);
                    if (norm > 1e-6f) {
                        for (float& value : vector)
                            value /= norm;
                    }
                    out[b] = std::move(vector);
                }
                m_dimension = static_cast<int>(hidden);
            }
        } else {
            error = m_error;
            ok = false;
        }
        if (info)
            m_api->ReleaseTensorTypeAndShapeInfo(info);
        if (output)
            m_api->ReleaseValue(output);
        return ok;
    }

    std::string m_dir;
    std::string m_error;
    std::string m_modelName;
    std::string m_outputName;
    std::vector<std::string> m_inputNames;
    std::unordered_map<std::string, int64_t> m_vocab;
    int64_t m_clsId = -1;
    int64_t m_sepId = -1;
    int64_t m_unkId = -1;
    int64_t m_padId = 0;
    int m_maxLength = 192;
    int m_dimension = 0;
    bool m_loaded = false;

    HMODULE m_ort = nullptr;
    const OrtApi* m_api = nullptr;
    OrtEnv* m_env = nullptr;
    OrtSessionOptions* m_options = nullptr;
    OrtSession* m_session = nullptr;
    OrtMemoryInfo* m_memory = nullptr;
};

} // namespace finder
