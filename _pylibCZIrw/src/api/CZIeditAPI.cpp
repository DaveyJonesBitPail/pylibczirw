#include "CZIeditAPI.h"
#include "CZIwriteAPI.h"

using namespace libCZI;
using namespace std;

CZIeditAPI::CZIeditAPI(const std::wstring &fileName) {
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
    throw std::logic_error("ReaderWriter is not initialized.");
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
    throw std::logic_error("ReaderWriter is not initialized.");
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

libCZI::ScalingInfo CZIeditAPI::ReadScalingInfo() const {
  ScalingInfo result{};
  if (!this->spReaderWriter_) {
    throw std::logic_error("ReaderWriter is not initialized.");
  }

  if (auto mdSeg = this->spReaderWriter_->ReadMetadataSegment()) {
    auto meta = CreateMetaFromMetadataSegment(mdSeg.get());
    auto documentInfo = meta->GetDocumentInfo();
    if (!documentInfo) {
      throw std::logic_error(
          "Unable to get document information from metadata.");
    }

    result = documentInfo->GetScalingInfo();
  }

  return result;
}

libCZI::CustomValueVariant
CZIeditAPI::ReadCustomKeyValue(const std::string &key) const {
  libCZI::CustomValueVariant result;

  if (!this->spReaderWriter_) {
    throw std::logic_error("ReaderWriter is not initialized.");
  }

  auto mdSeg = this->spReaderWriter_->ReadMetadataSegment();
  if (!mdSeg) {
    throw std::logic_error("Unable to read the metadata segment.");
  }

  auto meta = CreateMetaFromMetadataSegment(mdSeg.get());
  auto rootRo = meta->GetChildNodeReadonly("ImageDocument");
  if (!rootRo) {
    throw std::logic_error("Unable to find the root node.");
  }

  std::string path =
      "ImageDocument/Metadata/Information/CustomAttributes/KeyValue/";
  path += key;

  auto kvNode = meta->GetChildNodeReadonly(path.c_str());
  if (!kvNode) {
    return result;  // not found
  }

  std::wstring typeAttrW;
  const bool hasType = kvNode->TryGetAttribute(L"Type", &typeAttrW);

  if (!hasType) {
    std::wstring wval;
    if (kvNode->TryGetValue(&wval)) {
      result.SetString(Utils::ConvertToUtf8(wval));
    }
    return result;
  }

  if (typeAttrW == L"Boolean") {
    bool b;
    if (kvNode->TryGetValueAsBool(&b)) {
      result.SetBool(b);
    }
    return result;
  }
  if (typeAttrW == L"Int32") {
    std::int32_t i;
    if (kvNode->TryGetValueAsInt32(&i)) {
      result.SetInt32(i);
    }
    return result;
  }
  if (typeAttrW == L"Double") {
    double d;
    if (kvNode->TryGetValueAsDouble(&d)) {
      result.SetDouble(d);
    }
    return result;
  }
  if (typeAttrW == L"Float") {
    float f;
    if (kvNode->TryGetValueAsFloat(&f)) {
      result.SetFloat(f);
    }
    return result;
  }
  if (typeAttrW == L"String") {
    std::wstring wval;
    if (kvNode->TryGetValue(&wval)) {
      result.SetString(Utils::ConvertToUtf8(wval));
    }
    return result;
  }

  std::wstring wval;
  if (kvNode->TryGetValue(&wval)) {
    result.SetString(Utils::ConvertToUtf8(wval));
  }
  return result;
}

static std::shared_ptr<IXmlNodeRw> GetChannelsNode(IXmlNodeRw *root) {
  auto displayNode = root->GetChildNode("Metadata/DisplaySetting");
  if (!displayNode) {
    return nullptr;
  }
  return displayNode->GetChildNode("Channels");
}

std::map<int, const ChannelDisplaySettingsStructWithNameAndDescription>
CZIeditAPI::ReadDisplaySettings() const {
  std::map<int, const ChannelDisplaySettingsStructWithNameAndDescription>
      result;

  if (!this->spReaderWriter_) {
    throw std::logic_error("ReaderWriter is not initialized.");
  }
  auto mdSeg = this->spReaderWriter_->ReadMetadataSegment();
  if (!mdSeg) {
    throw std::logic_error("Unable to read the metadata segment.");
  }

  auto meta = CreateMetaFromMetadataSegment(mdSeg.get());

  auto channelsNode = meta->GetChildNodeReadonly(
      "ImageDocument/Metadata/DisplaySetting/Channels");
  if (!channelsNode) {
    throw std::logic_error("Unable to find the channels node.");
  }

  int chanIdxEnum = -1;
  channelsNode->EnumChildren(
      [&](std::shared_ptr<IXmlNodeRead> child) -> bool {
        if (child && child->Name() == L"Channel") {
          ++chanIdxEnum;

          int channelIndex = chanIdxEnum;
          std::wstring idAttr;
          if (child->TryGetAttribute(L"Id", &idAttr)) {
            const std::wstring prefix = L"Channel:";
            if (idAttr.rfind(prefix, 0) == 0) {
              try {
                channelIndex = std::stoi(idAttr.substr(prefix.size()));
              } catch (...) {
                channelIndex = chanIdxEnum;
              }
            }
          }

          std::string path = "Channel[" + std::to_string(chanIdxEnum) + "]";
          auto chRw = channelsNode->GetChildNodeReadonly(path.c_str());
          if (!chRw) {
            return true;
          }

          ChannelDisplaySettingsStructWithNameAndDescription ds;
          ds.Clear();

          std::wstring nameAttr;
          if (chRw->TryGetAttribute(L"Name", &nameAttr)) {
            ds.name = nameAttr;
          }

          auto descNode = chRw->GetChildNodeReadonly("Description");
          if (descNode) {
            std::wstring descVal;
            if (descNode->TryGetValue(&descVal)) {
              ds.description = descVal;
            }
          }

          auto isSelNode = chRw->GetChildNodeReadonly("IsSelected");
          if (isSelNode) {
            std::wstring selVal;
            if (isSelNode->TryGetValue(&selVal)) {
              ds.isEnabled =
                  (selVal == L"true" || selVal == L"True" || selVal == L"1");
            }
          }

          result.emplace(channelIndex, ds);
        }
        return true;
      });

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

PyCZIMetadataBuilder::PyCZIMetadataBuilder(
    std::shared_ptr<ICziMetadataBuilder> builder,
    std::shared_ptr<ICziReaderWriter> readerWriter)
    : spBuilder_(std::move(builder)), wpReaderWriter_(readerWriter) {
  if (!spBuilder_) {
    throw std::invalid_argument("builder cannot be null");
  }
}

std::string PyCZIMetadataBuilder::GetXml(bool prettify) const {
  if (!this->spBuilder_) {
    throw std::logic_error("Builder is not initialized.");
  }
  return this->spBuilder_->GetXml(prettify);
}

void PyCZIMetadataBuilder::SetXml(const std::string &xmlString) {
  this->spBuilder_ = CreateMetadataBuilderFromXml(xmlString);
}

std::shared_ptr<IXmlNodeRw> PyCZIMetadataBuilder::GetRootNode() {
  if (!this->spBuilder_) {
    throw std::logic_error("Builder is not initialized.");
  }
  return this->spBuilder_->GetRootNode();
}

void PyCZIMetadataBuilder::SetGeneralDocumentInfo(
    const GeneralDocumentInfo &docInfo) {
  if (!this->spBuilder_) {
    throw std::logic_error("Builder is not initialized.");
  }
  MetadataUtils::WriteGeneralDocumentInfo(this->spBuilder_.get(), docInfo);
}

void PyCZIMetadataBuilder::SetScalingInfo(
    const libCZI::ScalingInfo &scalingInfo) {
  if (!this->spBuilder_) {
    throw std::logic_error("Builder is not initialized.");
  }
  MetadataUtils::WriteScalingInfo(this->spBuilder_.get(), scalingInfo);
}

void PyCZIMetadataBuilder::SetCustomKeyValue(
    const std::string &key, const libCZI::CustomValueVariant &value) {
  if (!this->spBuilder_) {
    throw std::logic_error("Builder is not initialized.");
  }
  MetadataUtils::SetOrAddCustomKeyValuePair(this->spBuilder_.get(), key, value);
}

void PyCZIMetadataBuilder::SetDisplaySettings(
    const std::map<int, const ChannelDisplaySettingsStructWithNameAndDescription>
        &displaySettings) {
  if (!this->spBuilder_) {
    throw std::logic_error("Builder is not initialized.");
  }
  PyCZIMetadataBuilder::ApplyDisplaySettings(this->spBuilder_.get(),
                                             displaySettings);
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

/*static*/ void PyCZIMetadataBuilder::ApplyDisplaySettings(
    ICziMetadataBuilder *builder,
    const std::map<int, const ChannelDisplaySettingsStructWithNameAndDescription>
        &displaySettings) {
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

  auto findChannelById =
      [&channelsNode](const std::wstring &channelId)
      -> std::shared_ptr<IXmlNodeRw> {
    int idx = 0;
    int targetIdx = -1;

    channelsNode->EnumChildren(
        [&](std::shared_ptr<IXmlNodeRead> child) -> bool {
          if (child && child->Name() == L"Channel") {
            std::wstring idAttr;
            if (child->TryGetAttribute(L"Id", &idAttr) &&
                idAttr == channelId) {
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
    std::wstring channelId =
        std::wstring(L"Channel:") + std::to_wstring(chIndex);
    auto chRw = findChannelById(channelId);
    if (!chRw) {
      continue;
    }

    if (!ds.name.empty()) {
      auto nameNode = chRw->GetOrCreateChildNode("Name");
      nameNode->SetValue(ds.name.c_str());
    }

    if (!ds.description.empty()) {
      auto descNode = chRw->GetOrCreateChildNode("Description");
      descNode->SetValue(ds.description.c_str());
    }

    {
      auto isSelectedNode = chRw->GetOrCreateChildNode("IsSelected");
      if (isSelectedNode) {
        isSelectedNode->SetValue(ds.isEnabled ? L"true" : L"false");
      }
    }
  }
}
