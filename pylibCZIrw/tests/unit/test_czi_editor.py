"""Module implementing unit tests for the CziEditor class"""

import types
import pytest

from pylibCZIrw import czi as czi_mod


class FakeBuilder:
    """Fake builder for testing CziMetadataBuilder wrapper."""

    def __init__(self):
        self._xml = "<ImageDocument/>"
        self._gdi = {}
        self._ds = {}
        self._sc = {}
        self._kv = None

    def get_xml(self, prettify: bool = False):  # pylint: disable=unused-argument
        """Return the stored XML."""
        return self._xml

    def set_xml(self, xml: str):
        """Set the XML content."""
        self._xml = xml

    def set_general_document_info(self, **kwargs):
        """Store general document info."""
        self._gdi.update(kwargs)

    def set_display_settings(self, mapping):
        """Store display settings."""
        self._ds.update(mapping)

    def set_scaling_info(self, **kwargs):
        """Store scaling info."""
        self._sc.update(kwargs)

    def set_custom_key_value(self, key, value):
        """Store custom key-value pair."""
        self._kv = (key, value)

    @staticmethod
    def can_commit():
        """Return True (always committable in tests)."""
        return True

    @staticmethod
    def commit():
        """No-op commit for testing."""
        return None


class FakeEditor:
    """Fake editor for testing CziEditor wrapper."""

    def __init__(self, path: str):
        self._path = path
        self._closed = False
        self._xml = '<?xml version="1.0"?>\n<ImageDocument></ImageDocument>'
        self._info = {"title": "T", "user_name": "U"}

    def is_open(self):
        """Return True if not closed."""
        return not self._closed

    def close(self):
        """Mark as closed."""
        self._closed = True

    def read_metadata_xml(self):
        """Return stored XML."""
        return self._xml

    def read_general_document_info(self):
        """Return stored info dict."""
        return dict(self._info)

    def create_metadata_builder(self):
        """Create a FakeBuilder or raise if closed."""
        if self._closed:
            raise RuntimeError("not open")
        return FakeBuilder()


def test_is_open_delegates(monkeypatch):
    """Test that is_open property delegates to native editor."""
    monkeypatch.setattr(czi_mod, "_pylibCZIrw", types.SimpleNamespace(czi_editor=FakeEditor))
    ed = czi_mod.CziEditor("dummy.czi")
    assert ed.is_open is True
    ed.close()
    assert ed.is_open is False


def test_read_metadata_xml_delegates(monkeypatch):
    """Test that read_metadata_xml delegates to native editor."""
    monkeypatch.setattr(czi_mod, "_pylibCZIrw", types.SimpleNamespace(czi_editor=FakeEditor))
    with czi_mod.edit_czi("x.czi") as ed:
        xml = ed.read_metadata_xml()
        assert xml.startswith("<?xml")
        assert "<ImageDocument" in xml


def test_read_general_document_info_dict(monkeypatch):
    """Test that read_general_document_info returns a dict."""
    monkeypatch.setattr(czi_mod, "_pylibCZIrw", types.SimpleNamespace(czi_editor=FakeEditor))
    ed = czi_mod.CziEditor("dummy.czi")
    info = ed.read_general_document_info()
    assert isinstance(info, dict)
    assert info["title"] == "T"
    assert info["user_name"] == "U"


def test_create_metadata_builder_exposes_expected_api(monkeypatch):
    """Test that create_metadata_builder returns an object with expected methods."""
    monkeypatch.setattr(czi_mod, "_pylibCZIrw", types.SimpleNamespace(czi_editor=FakeEditor))
    ed = czi_mod.CziEditor("dummy.czi")
    b = ed.create_metadata_builder()
    for name in (
        "get_xml",
        "set_xml",
        "set_general_document_info",
        "set_display_settings",
        "set_scaling_info",
        "set_custom_key_value",
        "can_commit",
        "commit",
    ):
        assert hasattr(b, name)


def test_make_channel_display_setting_with_description_sets_fields(monkeypatch):
"""Test that make_channel_display_setting_with_description sets all fields correctly."""

class FakeStruct:
    """Fake struct for testing display settings."""

    def __init__(self):
        class Color:  # pylint: disable=missing-class-docstring
            def __init__(self):
                self.r = None
                self.g = None
                self.b = None
        self._cleared = False
        self.isEnabled = None
        self.blackPoint = None
        self.whitePoint = None
        self.tintingColor = Color()
        self.tintingMode = None
        self.description = None

    def Clear(self):  # pylint: disable=invalid-name
        """Mark as cleared."""
        self._cleared = True

    TintEnum = types.SimpleNamespace(Color=object(), **{"None": object()})
    fake_ns = types.SimpleNamespace(
        ChannelDisplaySettingsStructWithDescription=FakeStruct,
        TintingModeEnum=TintEnum,
    )
    monkeypatch.setattr(czi_mod, "_pylibCZIrw", fake_ns)

    color = czi_mod.Rgb8Color(255, 10, 5)
    ds = czi_mod.CziEditor.make_channel_display_setting_with_description(
        is_enabled=True,
        tinting_mode=czi_mod.TintingMode.Color,
        tinting_color=color,
        black_point=0.1,
        white_point=0.9,
        description="desc",
    )
    assert ds._cleared is True
    assert ds.isEnabled is True
    assert ds.blackPoint == 0.1
    assert ds.whitePoint == 0.9
    assert ds.tintingColor.r == 255
    assert ds.tintingColor.g == 10
    assert ds.tintingColor.b == 5
    assert ds.tintingMode is TintEnum.Color
    assert ds.description == "desc"

    ds2 = czi_mod.CziEditor.make_channel_display_setting_with_description(
        is_enabled=False,
        tinting_mode=czi_mod.TintingMode.none,
        tinting_color=color,
        black_point=0.0,
        white_point=1.0,
        description="",
    )
    assert ds2.tintingMode is getattr(TintEnum, "None")


def test_edit_czi_context_manager_closes(monkeypatch):
    """Test that edit_czi context manager properly closes the editor."""
    monkeypatch.setattr(czi_mod, "_pylibCZIrw", types.SimpleNamespace(czi_editor=FakeEditor))
    with czi_mod.edit_czi("x.czi") as ed:
        assert ed.is_open
    ed.close()
    assert ed.is_open is False


def test_create_metadata_builder_raises_when_not_open(monkeypatch):
    """Test that create_metadata_builder raises when editor is closed."""

    class RaisingEditor(FakeEditor):  # pylint: disable=missing-class-docstring
        def create_metadata_builder(self):
            """Raise RuntimeError to simulate closed editor."""
            raise RuntimeError("not open")

    monkeypatch.setattr(czi_mod, "_pylibCZIrw", types.SimpleNamespace(czi_editor=RaisingEditor))
    ed = czi_mod.CziEditor("dummy.czi")
    with pytest.raises(RuntimeError):
        ed.create_metadata_builder()
