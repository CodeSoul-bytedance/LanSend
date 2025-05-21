#pragma once

#include <algorithm>
#include <filesystem>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string_view>

namespace lansend::core {

enum class FileType {
    kImage,
    kVideo,
    kAudio,
    kText,
    kPDF,
    kArchive,
    kDocument,
    kPresentation,
    kSpreadsheet,
    kOther,
};

NLOHMANN_JSON_SERIALIZE_ENUM(FileType,
                             {
                                 {FileType::kImage, "Image"},
                                 {FileType::kVideo, "Video"},
                                 {FileType::kAudio, "Audio"},
                                 {FileType::kText, "Text"},
                                 {FileType::kPDF, "PDF"},
                                 {FileType::kArchive, "Archive"},
                                 {FileType::kDocument, "Document"},
                                 {FileType::kPresentation, "Presentation"},
                                 {FileType::kSpreadsheet, "Spreadsheet"},
                                 {FileType::kOther, "Other"},
                             })

inline std::string_view FileTypeToString(FileType type) {
    switch (type) {
    case FileType::kImage:
        return "Image";
    case FileType::kVideo:
        return "Video";
    case FileType::kAudio:
        return "Audio";
    case FileType::kText:
        return "Text";
    case FileType::kPDF:
        return "PDF";
    case FileType::kArchive:
        return "Archive";
    case FileType::kDocument:
        return "Document";
    case FileType::kPresentation:
        return "Presentation";
    case FileType::kSpreadsheet:
        return "Spreadsheet";
    case FileType::kOther:
        return "Other";
    }
    return "Unknown";
}

inline FileType GetFileType(std::string_view filepath) {
    std::filesystem::path path(filepath);
    std::string ext = path.extension().string();

    // If there's no extension or it's just a dot
    if (ext.empty() || ext == ".") {
        return FileType::kOther;
    }

    // Remove the leading dot from the extension
    ext = ext.substr(1);

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    // Image formats
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" || ext == "bmp"
        || ext == "tiff" || ext == "webp" || ext == "svg" || ext == "ico" || ext == "heic"
        || ext == "heif" || ext == "raw") {
        return FileType::kImage;
    }

    // Video formats
    if (ext == "mp4" || ext == "avi" || ext == "mkv" || ext == "mov" || ext == "wmv" || ext == "flv"
        || ext == "webm" || ext == "m4v" || ext == "mpg" || ext == "mpeg" || ext == "3gp") {
        return FileType::kVideo;
    }

    // Audio formats
    if (ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "flac" || ext == "aac"
        || ext == "wma" || ext == "m4a" || ext == "opus" || ext == "mid") {
        return FileType::kAudio;
    }

    // Text formats
    if (ext == "txt" || ext == "md" || ext == "csv" || ext == "json" || ext == "xml"
        || ext == "yaml" || ext == "yml" || ext == "ini" || ext == "log") {
        return FileType::kText;
    }

    // PDF
    if (ext == "pdf") {
        return FileType::kPDF;
    }

    // Archive formats
    if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz" || ext == "bz2"
        || ext == "xz" || ext == "tgz" || ext == "iso") {
        return FileType::kArchive;
    }

    // Document formats
    if (ext == "doc" || ext == "docx" || ext == "odt" || ext == "rtf" || ext == "tex"
        || ext == "epub") {
        return FileType::kDocument;
    }

    // Presentation formats
    if (ext == "ppt" || ext == "pptx" || ext == "odp" || ext == "key" || ext == "pps") {
        return FileType::kPresentation;
    }

    // Spreadsheet formats
    if (ext == "xls" || ext == "xlsx" || ext == "ods" || ext == "numbers" || ext == "xlsm") {
        return FileType::kSpreadsheet;
    }

    // If no match, return Other
    return FileType::kOther;
}

} // namespace lansend::core