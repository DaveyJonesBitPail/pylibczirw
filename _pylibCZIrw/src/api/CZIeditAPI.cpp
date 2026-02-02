#include "CZIeditAPI.h"
#include "CZIwriteAPI.h"

using namespace libCZI;
using namespace std;

CZIeditAPI::CZIeditAPI(const std::wstring& fileName) {
    auto stream = CreateInputOutputStreamForFile(fileName.c_str());
    auto rw = CreateCZIReaderWriter();
    rw->Create(stream);
    this->spReaderWriter_ = rw;
}

void CZIeditAPI::Close() {
    if (this->spReaderWriter_) {
        this->spReaderWriter_->Close();
        this->spReaderWriter_.reset();
    }
}

std::string CZIeditAPI::ReadMetadataXml() const {
    if (!this->spReaderWriter_) {
        return std::string();
    }
    if (auto mdSeg = this->spReaderWriter_->ReadMetadataSegment()) {
        auto meta = CreateMetaFromMetadataSegment(mdSeg.get());
        return meta->GetXml();
    }
    return std::string();
}

libCZI::GeneralDocumentInfo CZIeditAPI::ReadGeneralDocumentInfo() const {
    GeneralDocumentInfo result;
    if (!this->spReaderWriter_) {
        return result;
    }
    if (auto mdSeg = this->spReaderWriter_->ReadMetadataSegment()) {
        auto meta = CreateMetaFromMetadataSegment(mdSeg.get());
        auto docInfo = meta->GetDocumentInfo();
        if (docInfo) {
            return docInfo->GetGeneralDocumentInfo();
        }
    }
    return result;
}

std::shared_ptr<PyCZIMetadataBuilder> CZIeditAPI::CreateMetadataBuilder() {
    if (!this->spReaderWriter_) {
        throw std::logic_error("CZIeditAPI is not open.");
    }

    std::shared_ptr<ICziMetadataBuilder> libCziBuilder;

    if (auto mdSegExisting = this->spReaderWriter_->ReadMetadataSegment()) {
        auto metaExisting = CreateMetaFromMetadataSegment(mdSegExisting.get());
        libCziBuilder = CreateMetadataBuilderFromXml(metaExisting->GetXml());
    } else {
        libCziBuilder = libCZI::CreateMetadataBuilder();
    }

    return std::shared_ptr<PyCZIMetadataBuilder>(
        new PyCZIMetadataBuilder(libCziBuilder, this->spReaderWriter_));
}

PyCZIMetadataBuilder::PyCZIMetadataBuilder(std::shared_ptr<ICziMetadataBuilder> builder,
                                           std::shared_ptr<ICziReaderWriter> readerWriter)
    : spBuilder_(std::move(builder)), wpReaderWriter_(readerWriter) {
    if (!spBuilder_) {
        throw std::invalid_argument("builder cannot be null");
    }
}

std::string PyCZIMetadataBuilder::GetXml(bool prettify) const {
    if (!this->spBuilder_) {
        return std::string();
    }
    return this->spBuilder_->GetXml(prettify);
}

void PyCZIMetadataBuilder::SetXml(const std::string& xmlString) {
    this->spBuilder_ = CreateMetadataBuilderFromXml(xmlString);
}

std::shared_ptr<IXmlNodeRw> PyCZIMetadataBuilder::GetRootNode() {
    if (!this->spBuilder_) {
        throw std::logic_error("Builder is not initialized.");
    }
    return this->spBuilder_->GetRootNode();
}

void PyCZIMetadataBuilder::SetGeneralDocumentInfo(const GeneralDocumentInfo& docInfo) {
    if (!this->spBuilder_) {
        throw std::logic_error("Builder is not initialized.");
    }
    MetadataUtils::WriteGeneralDocumentInfo(this->spBuilder_.get(), docInfo);
}

void PyCZIMetadataBuilder::SetScalingInfo(const libCZI::ScalingInfo& scalingInfo) {
    if (!this->spBuilder_) {
        throw std::logic_error("Builder is not initialized.");
    }
    MetadataUtils::WriteScalingInfo(this->spBuilder_.get(), scalingInfo);
}

void PyCZIMetadataBuilder::SetCustomKeyValue(const std::string& key, const libCZI::CustomValueVariant& value) {
    if (!this->spBuilder_) {
        throw std::logic_error("Builder is not initialized.");
    }
    MetadataUtils::SetOrAddCustomKeyValuePair(this->spBuilder_.get(), key, value);
}

void PyCZIMetadataBuilder::SetDisplaySettings(
    const std::map<int, const ChannelDisplaySettingsStructWithDescription>& displaySettings) {
    if (!this->spBuilder_) {
        throw std::logic_error("Builder is not initialized.");
    }
    PyCZIMetadataBuilder::ApplyDisplaySettings(this->spBuilder_.get(), displaySettings);
}

bool PyCZIMetadataBuilder::CanCommit() const {
    return !this->wpReaderWriter_.expired() && this->spBuilder_ != nullptr;
}

void PyCZIMetadataBuilder::Commit() {
    auto spReaderWriter = this->wpReaderWriter_.lock();
    if (!spReaderWriter) {
        throw std::logic_error("Parent CZI file is no longer available.");
    }
    if (!this->spBuilder_) {
        throw std::logic_error("Builder is not initialized.");
    }

    const auto xmlOut = this->spBuilder_->GetXml(true);
    WriteMetadataInfo wmi;
    wmi.Clear();
    wmi.szMetadata = xmlOut.c_str();
    wmi.szMetadataSize = xmlOut.size();
    wmi.ptrAttachment = nullptr;
    wmi.attachmentSize = 0;
    spReaderWriter->SyncWriteMetadata(wmi);
}

static std::shared_ptr<IXmlNodeRw> GetOrCreateChannelsNode(IXmlNodeRw* root) {
    auto display = root->GetOrCreateChildNode("Metadata/DisplaySetting");
    return display->GetOrCreateChildNode("Channels");
}

static std::shared_ptr<IXmlNodeRw> GetOrCreateChannelById(
    std::shared_ptr<IXmlNodeRw> channels,
    const std::wstring& channelId,
    const std::wstring& channelName) {
    
    int targetIdx = -1;
    int idx = 0;
    channels->EnumChildren(
        [&](std::shared_ptr<IXmlNodeRead> child) -> bool {
            if (child && child->Name() == L"Channel") {
                std::wstring idAttr;
                if (child->TryGetAttribute(L"Id", &idAttr) && idAttr == channelId) {
                    targetIdx = idx;
                    return false;
                }
                ++idx;
            }
            return true;
        });

    if (targetIdx >= 0) {
        std::string path = "Channel[" + std::to_string(targetIdx) + "]";
        return channels->GetChildNode(path.c_str());
    }

    auto ch = channels->AppendChildNode("Channel");
    ch->SetAttribute(L"Id", channelId.c_str());
    if (!channelName.empty()) {
        ch->SetAttribute(L"Name", channelName.c_str());
    }
    return ch;
}

/*static*/ void PyCZIMetadataBuilder::ApplyDisplaySettings(
    ICziMetadataBuilder* builder,
    const std::map<int, const ChannelDisplaySettingsStructWithDescription>& displaySettings) {

    if (displaySettings.empty()) {
        return;
    }

    auto root = builder->GetRootNode();

    auto displayNode = root->GetChildNode("Metadata/DisplaySetting");
    if (!displayNode) {
        return;
    }
    auto channelsNode = displayNode->GetChildNode("Channels");
    if (!channelsNode) {
        return;
    }


    auto findChannelById = [&channelsNode](const std::wstring &channelId)
        -> std::shared_ptr<IXmlNodeRw> {
        int idx = 0;
        int targetIdx = -1;

        channelsNode->EnumChildren(
            [&](std::shared_ptr<IXmlNodeRead> child) -> bool {
                if (child && child->Name() == L"Channel") {
                    std::wstring idAttr;
                    if (child->TryGetAttribute(L"Id", &idAttr) && idAttr == channelId) {
                        targetIdx = idx;
                        return false;
                    }
                    ++idx;
                }
                return true;
            });

        if (targetIdx < 0) {
            return nullptr;
        }

        std::string path = "Channel[" + std::to_string(targetIdx) + "]";
        return channelsNode->GetChildNode(path.c_str());
    };

    for (const auto &entry : displaySettings) {
        const int chIndex = entry.first;
        const auto &ds = entry.second;

        // Only touch channels that already exist
        std::wstring channelId = std::wstring(L"Channel:") + std::to_wstring(chIndex);
        auto chRw = findChannelById(channelId);
        if (!chRw) {
            continue;
        }

        if (!ds.description.empty()) {
            auto descNode = chRw->GetChildNode("Description");
            if (!descNode) {
                descNode = chRw->AppendChildNode("Description");
            }
            descNode->SetValue(ds.description.c_str());
        }

        {
            auto isSelectedNode = chRw->GetChildNode("IsSelected");
            if (isSelectedNode) {
                isSelectedNode->SetValue(ds.isEnabled ? L"true" : L"false");
            }
        }
    }
}
