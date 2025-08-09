#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

constexpr size_t NONCE_SIZE = 0xC;
constexpr size_t TRAILER_SIZE = 0x40;
constexpr uint32_t CHUNK_SIZE = 0xFFFFFFFF;
constexpr uint32_t DEFAULT_MOD_CHUNK_SIZE = 65536;

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    CryptoPP::HexDecoder decoder;
    decoder.Put((CryptoPP::byte*)hex.data(), hex.size());
    decoder.MessageEnd();

    uint8_t byte;
    while (decoder.Get(byte)) {
        bytes.push_back(byte);
    }
    return bytes;
}

std::string sha1Hex(const std::vector<uint8_t>& data) {
    CryptoPP::SHA1 hash;
    std::string digest;
    CryptoPP::StringSource ss(data.data(), data.size(), true,
        new CryptoPP::HashFilter(hash,
            new CryptoPP::HexEncoder(new CryptoPP::StringSink(digest), false)));
    return digest;
}

std::vector<uint8_t> gcmEncrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce,
    const std::vector<uint8_t>& aad) {

    CryptoPP::GCM<CryptoPP::AES>::Encryption enc;
    enc.SetKeyWithIV(key.data(), key.size(), nonce.data(), nonce.size());

    std::string ciphertext;
    CryptoPP::AuthenticatedEncryptionFilter ef(enc,
        new CryptoPP::StringSink(ciphertext),
        false, 16);

    ef.ChannelPut("AAD", aad.data(), aad.size());
    ef.ChannelMessageEnd("AAD");

    ef.ChannelPut("", plaintext.data(), plaintext.size());
    ef.ChannelMessageEnd("");

    return std::vector<uint8_t>(ciphertext.begin(), ciphertext.end());
}

std::vector<uint8_t> gcmDecrypt(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce,
    const std::vector<uint8_t>& aad) {

    CryptoPP::GCM<CryptoPP::AES>::Decryption dec;
    dec.SetKeyWithIV(key.data(), key.size(), nonce.data(), nonce.size());

    std::string plaintext;
    CryptoPP::AuthenticatedDecryptionFilter df(dec,
        new CryptoPP::StringSink(plaintext),
        CryptoPP::AuthenticatedDecryptionFilter::DEFAULT_FLAGS,
        16);

    df.ChannelPut("AAD", aad.data(), aad.size());
    df.ChannelMessageEnd("AAD");

    df.ChannelPut("", ciphertext.data(), ciphertext.size());
    df.ChannelMessageEnd("");

    if (!df.GetLastResult()) {
        throw std::runtime_error("Decryption failed: Integrity check failed.");
    }

    return std::vector<uint8_t>(plaintext.begin(), plaintext.end());
}

std::vector<uint8_t> decryptBuildManifest(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> nonce(data.begin(), data.begin() + NONCE_SIZE);
    std::vector<uint8_t> ciphertext(data.begin() + NONCE_SIZE, data.end() - TRAILER_SIZE);
    return gcmDecrypt(ciphertext, key, nonce, {'b','u','i','l','d','-','m','a','n','i','f','e','s','t'});
}

std::vector<uint8_t> encryptBuildManifest(const std::vector<uint8_t>& jsonData, const std::vector<uint8_t>& key) {
    CryptoPP::AutoSeededRandomPool rng;
    std::vector<uint8_t> nonce(NONCE_SIZE);
    rng.GenerateBlock(nonce.data(), nonce.size());

    auto ciphertext = gcmEncrypt(jsonData, key, nonce, {'b','u','i','l','d','-','m','a','n','i','f','e','s','t'});

    std::vector<uint8_t> result;
    result.reserve(NONCE_SIZE + ciphertext.size() + TRAILER_SIZE);
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    result.insert(result.end(), TRAILER_SIZE, 0);
    return result;
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open file: " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to write file: " + path);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void appendModFiles(json& manifest) {
    fs::path modDir = "modarchives";
    if (!fs::exists(modDir) || !fs::is_directory(modDir)) return;

    for (const auto& entry : fs::directory_iterator(modDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".resources") {
            std::string fileName = entry.path().filename().string();
            std::string manifestPath = "modarchives/" + fileName;
            std::vector<uint8_t> fileData = readFile(entry.path().string());
            std::string sha1 = sha1Hex(fileData);
            uintmax_t size = fileData.size();
            size_t numHashes = (size + DEFAULT_MOD_CHUNK_SIZE - 1) / DEFAULT_MOD_CHUNK_SIZE;

            manifest["files"][manifestPath] = {
                {"fileSize", size},
                {"chunkSize", DEFAULT_MOD_CHUNK_SIZE},
                {"hashes", std::vector<std::string>(numHashes, sha1)}
            };

            std::cout << "Added/Updated mod file: " << manifestPath << " (" << size << " bytes, SHA1: " << sha1 << ")\n";
        }
    }
}

std::vector<uint8_t> optimizeBuildManifest(const std::vector<uint8_t>& jsonData) {
    json manifest = json::parse(jsonData);
    if (!manifest.contains("files")) throw std::runtime_error("Invalid manifest format");

    for (auto& [path, file] : manifest["files"].items()) {
        if (fs::exists(path)) {
            auto size = fs::file_size(path);
            file["fileSize"] = size;
            std::cout << "Found file: " << path << ", fileSize updated to: " << size << std::endl;

            size_t numHashes = (size + CHUNK_SIZE - 1) / CHUNK_SIZE;
            file["hashes"] = std::vector<std::string>(numHashes, "e2df1b2aa831724ec987300f0790f04ad3f5beb8");
            file["chunkSize"] = CHUNK_SIZE;
        }
    }

    appendModFiles(manifest);

    std::string out = manifest.dump();
    return std::vector<uint8_t>(out.begin(), out.end());
}

std::vector<uint8_t> injectModsOnly(const std::vector<uint8_t>& jsonData) {
    json manifest = json::parse(jsonData);
    appendModFiles(manifest);
    std::string out = manifest.dump();
    return std::vector<uint8_t>(out.begin(), out.end());
}


int main(int argc, char* argv[]) {
    std::cout << "DEternal_patchManifest (cpp port) by ImNotSimon\n\n";

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <AES 128-bit hex key>\n";
        return 1;
    }

    std::vector<uint8_t> key;
    try {
        key = hexToBytes(argv[1]);
        if (key.size() != 0x10) throw std::runtime_error("Key length != 128 bits");
    }
    catch (...) {
        std::cerr << "Invalid AES key. Must be 32 hex chars (128 bits).\n";
        return 1;
    }

    try {
        auto encData = readFile("build-manifest.bin");
        auto decrypted = decryptBuildManifest(encData, key);
        auto optimized = optimizeBuildManifest(decrypted);
        auto reenc = encryptBuildManifest(optimized, key);
        writeFile("build-manifest.bin", reenc);
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
