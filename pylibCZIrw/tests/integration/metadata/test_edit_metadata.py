"""Integration tests for in-place metadata editing via CziEditor."""

import hashlib
import os
import shutil

import defusedxml.ElementTree as ET
import pytest
import xmltodict

from pylibCZIrw.czi import (
    ChannelDisplaySettingsDataClassWithNameAndDescription,
    Rgb8Color,
    TintingMode,
    edit_czi,
    open_czi,
)

working_dir = os.path.dirname(os.path.abspath(__file__))
CZI_DOCUMENT_TEST1 = os.path.join(working_dir, "test_data", "c1_bgr24_t1_z1_h1.czi")


def _sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


@pytest.fixture
def czi_working_copy(tmp_path) -> str:
    src = CZI_DOCUMENT_TEST1
    dst = tmp_path / "work.czi"
    shutil.copy2(src, dst)
    pytest.original_hash = _sha256(src)
    return str(dst)


def test_editor_open_close(czi_working_copy: str) -> None:
    """Ensure editor opens and closes an existing CZI."""
    with edit_czi(czi_working_copy) as editor:
        assert editor.is_open
        xml = editor.read_metadata_xml()
        xmltree = ET.ElementTree(ET.fromstring(xml))
        assert xmltree.getroot().tag == "ImageDocument"
    assert _sha256(CZI_DOCUMENT_TEST1) == pytest.original_hash


def test_read_general_document_info_has_valid_fields(czi_working_copy: str) -> None:
    """GeneralDocumentInfo readback returns a dict with only valid fields populated."""
    with edit_czi(czi_working_copy) as editor:
        info = editor.read_general_document_info()
        assert isinstance(info, dict)


def test_builder_commit_updates_document_info(czi_working_copy: str) -> None:
    """Modify GeneralDocumentInfo via builder and commit."""
    with edit_czi(czi_working_copy) as editor:
        builder = editor.create_metadata_builder()
        xml_before = builder.get_xml(prettify=True)
        assert "ImageDocument" in xml_before

        builder.set_general_document_info(
            title="In-place Title Test",
            user_name="EditorUser",
            comment="Edited by editor test",
        )
        assert builder.can_commit()
        builder.commit()

        xml_after = editor.read_metadata_xml()
        meta_dict = xmltodict.parse(xml_after)
        info_node = meta_dict["ImageDocument"]["Metadata"]["Information"]
        assert info_node["Document"]["Title"] == "In-place Title Test"
        assert "UserName" in info_node["Document"] and info_node["Document"]["UserName"] == "EditorUser"
        assert info_node["Document"]["Comment"] == "Edited by editor test"


def test_builder_updates_existing_display_channels_only(czi_working_copy: str) -> None:
    """Update DisplaySetting/Channels for existing channels without overwriting entire structure."""
    with open_czi(czi_working_copy) as reader:
        raw = reader.raw_metadata
        meta = xmltodict.parse(raw)
        ds_channels = (
            meta.get("ImageDocument", {})
            .get("Metadata", {})
            .get("DisplaySetting", {})
            .get("Channels", {})
            .get("Channel", [])
        )
        if isinstance(ds_channels, dict):
            ds_channels = [ds_channels]
        existing_ids = []
        for i, ch in enumerate(ds_channels):
            cid = ch.get("@Id")
            if cid:
                if cid.startswith("Channel:"):
                    try:
                        idx = int(cid.split(":")[1])
                    except Exception:
                        idx = i
                else:
                    idx = i
                existing_ids.append(idx)

    if not existing_ids:
        pytest.skip("Asset has no DisplaySetting/Channels; skipping.")

    with edit_czi(czi_working_copy) as editor:
        builder = editor.create_metadata_builder()

        first_idx = existing_ids[0]
        red = Rgb8Color(255, 0, 0)
        ds = ChannelDisplaySettingsDataClassWithNameAndDescription(
            is_enabled=True,
            tinting_mode=TintingMode.Color,
            tinting_color=red,
            black_point=0.05,
            white_point=0.95,
            description="Edited by test",
        )

        builder.set_display_settings({first_idx: ds})
        assert builder.can_commit()
        builder.commit()

        xml_after = editor.read_metadata_xml()
        parsed = xmltodict.parse(xml_after)

        channels = (
            parsed.get("ImageDocument", {})
            .get("Metadata", {})
            .get("DisplaySetting", {})
            .get("Channels", {})
            .get("Channel", [])
        )
        if isinstance(channels, dict):
            channels = [channels]

        def find_channel(ch_list, target_idx):
            for ch in ch_list:
                cid = ch.get("@Id", "")
                if cid == f"Channel:{target_idx}":
                    return ch
            return None

        ch_after = find_channel(channels, first_idx)
        assert ch_after is not None

        assert ch_after.get("Description") == "Edited by test"
        if "IsSelected" in ch_after:
            assert ch_after["IsSelected"] in ("true", "false")
        assert "BitCountRange" in ch_after or "PixelType" in ch_after

        ds_map = editor.read_display_settings()
        assert isinstance(ds_map, dict)


def test_builder_set_xml_roundtrip(czi_working_copy: str) -> None:
    """SetXml replaces builder content; Commit writes it back."""
    with edit_czi(czi_working_copy) as editor:
        builder = editor.create_metadata_builder()
        xml = builder.get_xml(prettify=True)
        assert "<ImageDocument" in xml

        builder.set_xml(xml)
        assert builder.can_commit()
        builder.commit()

        xml_after = editor.read_metadata_xml()
        xmltree = ET.ElementTree(ET.fromstring(xml_after))
        assert xml_after.lstrip().startswith("<?xml")
        assert xmltree.getroot().tag == "ImageDocument"


def test_editor_multiple_commits(czi_working_copy: str) -> None:
    """Multiple commits should succeed, keeping structure intact."""
    with edit_czi(czi_working_copy) as editor:
        builder = editor.create_metadata_builder()

        builder.set_general_document_info(comment="Commit 1")
        builder.commit()

        builder.set_general_document_info(comment="Commit 2")
        builder.commit()

        xml = editor.read_metadata_xml()
        parsed = xmltodict.parse(xml)
        comment = (
            parsed.get("ImageDocument", {})
            .get("Metadata", {})
            .get("Information", {})
            .get("Document", {})
            .get("Comment", "")
        )
        assert comment == "Commit 2"


def test_set_general_document_info_partial_fields_preserves_others(czi_working_copy: str) -> None:
    with edit_czi(czi_working_copy) as editor:
        before = xmltodict.parse(editor.read_metadata_xml())
        prev_doc = before.get("ImageDocument", {}).get("Metadata", {}).get("Information", {}).get("Document", {})
        prev_title = prev_doc.get("Title")

        builder = editor.create_metadata_builder()
        builder.set_general_document_info(keywords="kw1, kw2")
        builder.commit()

        after = xmltodict.parse(editor.read_metadata_xml())
        doc = after["ImageDocument"]["Metadata"]["Information"]["Document"]
        assert doc.get("Keywords") == "kw1, kw2"
        assert doc.get("Title") == prev_title


def test_set_xml_invalid_content_raises(czi_working_copy: str) -> None:
    with edit_czi(czi_working_copy) as editor:
        builder = editor.create_metadata_builder()
        bad_xml = "<ImageDocument><Metadata></ImageDocument"
        with pytest.raises(Exception):
            builder.set_xml(bad_xml)


def test_set_display_settings_nonexistent_channel_noop(czi_working_copy: str) -> None:
    with edit_czi(czi_working_copy) as editor:
        builder = editor.create_metadata_builder()

        ds = ChannelDisplaySettingsDataClassWithNameAndDescription(
            is_enabled=True,
            tinting_mode=TintingMode.Color,
            tinting_color=Rgb8Color(0, 0, 0),
            black_point=0.0,
            white_point=1.0,
            description="SHOULD NOT BE ADDED",
        )

        builder.set_display_settings({9999: ds})
        builder.commit()

        xml_after = editor.read_metadata_xml()
        parsed = xmltodict.parse(xml_after)
        channels = (
            parsed.get("ImageDocument", {})
            .get("Metadata", {})
            .get("DisplaySetting", {})
            .get("Channels", {})
            .get("Channel", [])
        )
        if isinstance(channels, dict):
            channels = [channels]
        assert all(ch.get("@Id") != "Channel:9999" for ch in channels)


def test_set_custom_key_value_add_and_overwrite(czi_working_copy: str) -> None:
    with edit_czi(czi_working_copy) as editor:
        builder = editor.create_metadata_builder()

        builder.set_custom_key_value("TestKey", 42)
        builder.commit()

        xml = editor.read_metadata_xml()
        md = xmltodict.parse(xml)
        info = md.get("ImageDocument", {}).get("Metadata", {}).get("Information", {})
        custom = info.get("CustomAttributes", {}).get("KeyValue", {})
        assert "TestKey" in custom

        assert editor.read_custom_key_value("TestKey") == 42

        builder = editor.create_metadata_builder()
        builder.set_custom_key_value("TestKey", "Hello")
        builder.commit()

        xml2 = editor.read_metadata_xml()
        md2 = xmltodict.parse(xml2)
        info2 = md2.get("ImageDocument", {}).get("Metadata", {}).get("Information", {})
        custom2 = info2.get("CustomAttributes", {}).get("KeyValue", {})
        assert "TestKey" in custom2

        assert editor.read_custom_key_value("TestKey") == "Hello"


def test_set_scaling_info_partial_fields(czi_working_copy: str) -> None:
    with edit_czi(czi_working_copy) as editor:
        builder = editor.create_metadata_builder()
        builder.set_scaling_info(scale_x=0.123)
        builder.commit()

        xml = editor.read_metadata_xml()
        md = xmltodict.parse(xml)
        assert md.get("ImageDocument", {}).get("Metadata", {}) is not None

        dto = editor.read_scaling_info()
        assert hasattr(dto, "scale_x")
        assert hasattr(dto, "scale_y")
        assert hasattr(dto, "scale_z")


def test_commit_without_changes_no_op(czi_working_copy: str) -> None:
    with edit_czi(czi_working_copy) as editor:
        xml_before = editor.read_metadata_xml()
        builder = editor.create_metadata_builder()
        builder.commit()
        xml_after = editor.read_metadata_xml()
        assert xml_before == xml_after


def test_builder_can_commit_false_after_editor_closed(czi_working_copy: str) -> None:
    editor_ctx = edit_czi(czi_working_copy)
    editor = editor_ctx.__enter__()
    try:
        builder = editor.create_metadata_builder()
        editor.close()
        assert not builder.can_commit()
        with pytest.raises(Exception):
            builder.commit()
    finally:
        try:
            editor_ctx.__exit__(None, None, None)
        except RuntimeError:
            pass
