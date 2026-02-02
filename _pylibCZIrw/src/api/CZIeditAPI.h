#pragma once

#include "PImage.h"
#include "inc_libCzi.h"
#include "CZIwriteAPI.h"
#include <optional>
#include <iostream>
#include <memory>

/// This POD ("plain-old-data") structure is intended to capture all information
/// found inside an IChannelDisplaySetting-object and tack on descriptions. It allows for easy
/// modification of the information.
struct ChannelDisplaySettingsStructWithDescription : ChannelDisplaySettingsStruct {
    std::wstring description; ///< The description of the channel.

    /// Sets the structure to a defined standard value - not enabled, no tinting,
    /// linear gradation-curve and black-point to zero, white-point to one, and clears description.
    LIBCZI_API void Clear() {
        ChannelDisplaySettingsStruct::Clear();
        this->description.clear();
    }
};

// Forward declaration
class CZIeditAPI;

/// Class representing a Python-facing wrapper for metadata editing operations.
/// Changes are accumulated in memory and only written to the file when Commit() is called.
/// This class wraps libCZI::ICziMetadataBuilder and adds lifecycle management.
class PyCZIMetadataBuilder {
    friend class CZIeditAPI; // Allow CZIeditAPI to construct instances

private:
    std::shared_ptr<libCZI::ICziMetadataBuilder> spBuilder_;
    std::weak_ptr<libCZI::ICziReaderWriter> wpReaderWriter_;

    PyCZIMetadataBuilder(std::shared_ptr<libCZI::ICziMetadataBuilder> builder,
                         std::shared_ptr<libCZI::ICziReaderWriter> readerWriter);

public:
    ~PyCZIMetadataBuilder() = default;

    PyCZIMetadataBuilder(const PyCZIMetadataBuilder&) = delete;
    PyCZIMetadataBuilder& operator=(const PyCZIMetadataBuilder&) = delete;
    PyCZIMetadataBuilder(PyCZIMetadataBuilder&&) = default;
    PyCZIMetadataBuilder& operator=(PyCZIMetadataBuilder&&) = default;

    /// Get the current XML representation of the metadata (for inspection or advanced editing).
    /// \param prettify If true, the XML is formatted with indentation.
    /// \returns The XML string representing the current state of the metadata.
    std::string GetXml(bool prettify = false) const;

    /// Replace the entire metadata XML content. For advanced users who need full control.
    /// \param xmlString The new XML content (must be valid CZI metadata XML with "ImageDocument" root).
    /// \throws LibCZIXmlParseException if the XML is malformed or missing required root node.
    void SetXml(const std::string& xmlString);

    /// Get the root node for direct XML manipulation (advanced usage).
    /// \returns A shared pointer to the root XML node for read/write access.
    std::shared_ptr<libCZI::IXmlNodeRw> GetRootNode();

    /// Set/merge GeneralDocumentInfo fields into the metadata.
    /// Only fields that are marked as valid in docInfo will be written.
    /// \param docInfo The document info to merge.
    void SetGeneralDocumentInfo(const libCZI::GeneralDocumentInfo& docInfo);

    /// Set/merge display settings for specified channels.
    /// Existing channels not in the map are preserved; channels in the map are updated/created.
    /// \param displaySettings Map of channel index -> channel display settings.
    void SetDisplaySettings(const std::map<int, const ChannelDisplaySettingsStructWithDescription>& displaySettings);

    /// Set scaling information.
    /// \param scalingInfo The scaling information to write.
    void SetScalingInfo(const libCZI::ScalingInfo& scalingInfo);

    /// Set or add a custom key-value pair in the metadata.
    /// \param key The key name.
    /// \param value The value (variant type).
    void SetCustomKeyValue(const std::string& key, const libCZI::CustomValueVariant& value);

    /// Commit all pending changes to the CZI file.
    /// \throws std::logic_error if the parent CZI file is no longer available.
    void Commit();

    /// Check if the builder has a valid connection to the parent file.
    /// \returns True if Commit() can be called successfully.
    bool CanCommit() const;

private:
    /// Apply display settings to a builder (shared implementation).
    static void ApplyDisplaySettings(libCZI::ICziMetadataBuilder* builder,
        const std::map<int, const ChannelDisplaySettingsStructWithDescription>& displaySettings);
};

/// Class used to represent a CZI in-place editor object in pylibCZIrw.
/// Provides factory method for creating PyCZIMetadataBuilder instances.
class CZIeditAPI {
    std::shared_ptr<libCZI::ICziReaderWriter> spReaderWriter_;

public:
    /// Open an existing CZI file for in-place metadata editing.
    /// \param fileName Path to an existing CZI file.
    explicit CZIeditAPI(const std::wstring& fileName);

    ~CZIeditAPI() = default;

    CZIeditAPI(const CZIeditAPI&) = delete;
    CZIeditAPI& operator=(const CZIeditAPI&) = delete;
    CZIeditAPI(CZIeditAPI&&) = default;
    CZIeditAPI& operator=(CZIeditAPI&&) = default;

    /// Close the opened CZI file.
    void Close();

    /// Check if the file is open.
    /// \returns True if the file is open and operations can be performed.
    bool IsOpen() const { return spReaderWriter_ != nullptr; }

    /// Read the current metadata XML from the file (read-only snapshot).
    /// \returns XML string of the current metadata, empty if no metadata segment exists.
    std::string ReadMetadataXml() const;

    /// Read current GeneralDocumentInfo from the file.
    /// \returns The GeneralDocumentInfo object (default-initialized if no metadata segment exists).
    libCZI::GeneralDocumentInfo ReadGeneralDocumentInfo() const;

    /// Create a metadata builder initialized from the file's current metadata.
    /// The builder holds a copy of the metadata that can be modified independently.
    /// Call Commit() on the builder to write changes back to the file.
    /// \returns A shared pointer to the new metadata builder wrapper.
    /// \throws std::logic_error if the file is not open.
    std::shared_ptr<PyCZIMetadataBuilder> CreateMetadataBuilder();
};
