import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import Aurora 1.0

ApplicationWindow {
    id: win
    visible: true
    title: "Aurora Studio"
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    width: Math.min(1120, launchScreen.availableGeometry.width - 40)
    height: Math.min(740, launchScreen.availableGeometry.height - 32)
    minimumWidth: 540
    maximumWidth: 1120
    minimumHeight: 480
    maximumHeight: 740
    modality: Qt.NonModal

    property string page: "gallery"
    property var edit: ({})
    property bool dirty: JSON.stringify(edit) !== JSON.stringify(repo.draft)
    property int visibleCount: repo.themes.length
    property bool pendingClose: false
    property string pendingPage: ""

    function clone(o) { return JSON.parse(JSON.stringify(o)) }
    function color(key) { return edit.palette ? edit.palette[key] : "#101421" }
    function setColor(key, value) {
        let d = clone(edit)
        if (!d.palette) d.palette = {}
        d.palette[key] = value
        edit = d
    }
    function beginEdit() {
        repo.editSelected()
        edit = clone(repo.draft)
        page = "editor"
    }
    function cancelEdit() {
        if (dirty) discardDialog.open()
        else { repo.cancelDraft(); page = "gallery" }
    }
    function discardEdit() { discardDialog.close(); repo.cancelDraft(); page = "gallery" }
    function requestClose() {
        if (page === "editor" && dirty) { pendingClose = true; discardDialog.open() }
        else win.close()
    }
    function moveSelection(delta) {
        if (page === "gallery" && visibleCount > 0)
            repo.selected = Math.max(0, Math.min(visibleCount - 1, repo.selected + delta))
    }

    ThemeRepository {
        id: repo
        onNotice: msg => { toast.message = msg; toastTimer.restart() }
    }

    Component.onCompleted: {
        const area = launchScreen.availableGeometry
        width = Math.max(minimumWidth, Math.min(1120, area.width - 40))
        height = Math.max(minimumHeight, Math.min(740, area.height - 32))
        x = area.x + Math.round((area.width - width) / 2)
        y = area.y + Math.round((area.height - height) / 2)
    }

    // The window surface stays alpha blended; Hyprland supplies the backdrop blur.
    Rectangle {
        id: surface
        anchors.fill: parent
        radius: 28
        color: "#9910182b"
        border.width: 1
        border.color: "#526a83b9"
        clip: true

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#203b4269" }
                GradientStop { position: 0.44; color: "#11182944" }
                GradientStop { position: 1.0; color: "#0d122555" }
            }
        }
        Rectangle {
            width: parent.width * 0.52
            height: 180
            x: -60; y: -120
            radius: 100
            color: "#253d55a8"
            opacity: 0.2
        }
        Rectangle {
            width: parent.width * 0.38
            height: 150
            x: parent.width * 0.7; y: parent.height - 100
            radius: 90
            color: "#1c286e8a"
            opacity: 0.18
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 32
            anchors.rightMargin: 32
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            spacing: 0

            // Header
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                spacing: 8
                RowLayout {
                    spacing: 11
                    Rectangle {
                        width: 38; height: 38; radius: 13
                        color: "#2526395d"
                        border.color: "#596b8fd0"
                        Text { anchors.centerIn: parent; text: "✦"; color: "#d7cbff"; font.pixelSize: 21 }
                    }
                    Column {
                        spacing: -1
                        Text { text: "AURORA"; color: "#bfc5e4"; font.pixelSize: 9; font.letterSpacing: 2.3; font.weight: Font.DemiBold }
                        Text { text: "Studio"; color: "#f5f5fd"; font.pixelSize: 18; font.weight: Font.DemiBold }
                    }
                }
                Item { Layout.fillWidth: true }
                Row {
                    spacing: 5
                    Repeater {
                        model: [{label:"Gallery",key:"gallery"},{label:"Editor",key:"editor"},{label:"Presets",key:"presets"}]
                        delegate: Button {
                            required property var modelData
                            text: modelData.label
                            height: 36; padding: 12
                            onClicked: {
                                if (modelData.key === "editor" && win.page !== "editor") win.beginEdit()
                                else if (win.page === "editor" && win.dirty) { pendingPage = modelData.key; discardDialog.open() }
                                else win.page = modelData.key
                            }
                            background: Rectangle {
                                radius: 12
                                color: win.page === modelData.key ? "#46394769" : (parent.hovered ? "#25334b66" : "transparent")
                                border.width: win.page === modelData.key ? 1 : 0
                                border.color: "#647e92bd"
                                Behavior on color { ColorAnimation { duration: 140 } }
                            }
                            contentItem: Text {
                                text: parent.text
                                color: win.page === modelData.key ? "#f2edff" : "#abb4cb"
                                font.pixelSize: 13; font.weight: win.page === modelData.key ? Font.DemiBold : Font.Medium
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "×"; width: 38; height: 38; padding: 0
                    onClicked: win.requestClose()
                    background: Rectangle { radius: 12; color: parent.hovered ? "#463d2847" : "#1825324b"; border.color: "#38536683" }
                    contentItem: Text { text: parent.text; color: "#d6dced"; font.pixelSize: 25; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }

            // Gallery: the selected portrait occupies about 27% of the gallery width.
            Item {
                id: gallery
                visible: win.page === "gallery"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 260
                clip: true

                Repeater {
                    model: repo.themes
                    delegate: Item {
                        id: card
                        required property var modelData
                        required property int index
                        property int delta: {
                            let offset = index - repo.selected
                            if (offset > visibleCount / 2) offset -= visibleCount
                            if (offset < -visibleCount / 2) offset += visibleCount
                            return offset
                        }
                        property int distance: Math.abs(delta)
                        property bool active: delta === 0
                        property real cardHeight: Math.max(190, Math.min(372, gallery.height - 10))
                        property real cardWidth: Math.max(138, Math.min(280, gallery.width * 0.28, cardHeight * 0.75))
                        width: cardWidth
                        height: cardHeight
                        // Keep enough horizontal separation for the scaled covers' labels.
                        // The cards still overlap visually at the edges, preserving the cover-flow look.
                        x: gallery.width / 2 - width / 2 + delta * width * 0.94
                        y: (gallery.height - height) / 2 + distance * 9 + 4
                        z: 20 - distance
                        scale: active ? 1.0 : (distance === 1 ? 0.83 : (distance === 2 ? 0.69 : 0.57))
                        opacity: active ? 1.0 : (distance === 1 ? 0.82 : (distance === 2 ? 0.53 : 0.27))
                        visible: distance <= 3
                        transformOrigin: Item.Center
                        transform: Rotation {
                            id: carouselRotation
                            origin.x: card.width / 2; origin.y: card.height / 2
                            axis.x: 0; axis.y: 1; axis.z: 0
                            angle: Math.max(-29, Math.min(29, card.delta * -10))
                            Behavior on angle { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
                        }
                        Behavior on x { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
                        Behavior on y { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
                        Behavior on scale { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -5
                            radius: 26
                            color: card.active ? (card.modelData.palette.accent + "30") : "transparent"
                            opacity: card.active ? 0.8 : 0
                            layer.enabled: card.active
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: card.modelData.palette.accent
                                shadowBlur: 0.75
                                shadowOpacity: 0.42
                                shadowVerticalOffset: 2
                            }
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: 23
                            color: card.modelData.palette.background
                            border.width: 1.5
                            border.color: card.active ? "#d18f9fcf" : "#555e7292"
                            clip: true

                            Image {
                                id: artwork
                                anchors.fill: parent
                                source: "file://" + card.modelData.path.substring(0, card.modelData.path.lastIndexOf("/") + 1) + card.modelData.preview
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true; cache: true
                                opacity: card.active ? 1 : 0.92
                                layer.enabled: true
                            }
                            Image {
                                anchors.fill: parent
                                source: visibleCount ? "file://" + repo.themes[0].path.substring(0, repo.themes[0].path.lastIndexOf("/") + 1) + repo.themes[0].preview : ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true; cache: true
                                visible: artwork.status === Image.Error
                            }
                            Rectangle {
                                anchors.fill: parent; radius: parent.radius
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#12071020" }
                                    GradientStop { position: 0.42; color: "#10101a2b" }
                                    GradientStop { position: 0.68; color: "#750b1020" }
                                    GradientStop { position: 1.0; color: "#f00a0f1c" }
                                }
                            }
                            Rectangle {
                                anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 14
                                width: 46; height: 23; radius: 8
                                color: "#69303950"; border.color: "#55d4dcff"
                                visible: card.active
                                Text { anchors.centerIn: parent; text: "CURATED"; color: "#f1efff"; font.pixelSize: 8; font.letterSpacing: 1.1; font.weight: Font.DemiBold }
                            }
                            Column {
                                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                                anchors.leftMargin: 17; anchors.rightMargin: 17
                                anchors.bottomMargin: 16
                                Text {
                                    width: parent.width
                                    text: card.modelData.name
                                    color: "#ffffff"
                                    font.pixelSize: card.active ? 19 : 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: { repo.selected = card.index }
                                onDoubleClicked: { repo.selected = card.index; win.beginEdit() }
                            }
                        }
                    }
                }
                Button {
                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                    text: "‹"; enabled: repo.selected > 0; width: 40; height: 48; padding: 0
                    onClicked: win.moveSelection(-1)
                    background: Rectangle { radius: 15; color: parent.hovered ? "#4a364864" : "#ad141c30"; border.color: "#586e86a9" }
                    contentItem: Text { text: parent.text; color: parent.enabled ? "#f1f2fc" : "#71809c"; font.pixelSize: 34; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
                Button {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    text: "›"; enabled: repo.selected < visibleCount - 1; width: 40; height: 48; padding: 0
                    onClicked: win.moveSelection(1)
                    background: Rectangle { radius: 15; color: parent.hovered ? "#4a364864" : "#ad141c30"; border.color: "#586e86a9" }
                    contentItem: Text { text: parent.text; color: parent.enabled ? "#f1f2fc" : "#71809c"; font.pixelSize: 34; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }

            // The selected cover already carries its name, so keep the lower row focused
            // on description, palette, and pagination rather than repeating that heading.
            ColumnLayout {
                visible: win.page === "gallery"
                Layout.fillWidth: true
                Layout.preferredHeight: 69
                spacing: 4
                Text {
                    Layout.fillWidth: true
                    text: visibleCount ? repo.themes[repo.selected].description : "Create or import a preset to get started."
                    color: "#b6bfd2"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
                }
                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8
                    Repeater {
                        model: visibleCount ? [repo.themes[repo.selected].palette.accent, repo.themes[repo.selected].palette.secondary, repo.themes[repo.selected].palette.tertiary, repo.themes[repo.selected].palette.surface] : []
                        delegate: Rectangle { required property string modelData; width: 17; height: 17; radius: 8.5; color: modelData; border.color: "#80ffffff"; border.width: 1 }
                    }
                }
                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 6
                    Repeater {
                        model: visibleCount
                        delegate: Rectangle {
                            required property int index
                            width: index === repo.selected ? 22 : 5; height: 5; radius: 3
                            color: index === repo.selected ? "#c9baff" : "#7d89a2"
                            Behavior on width { NumberAnimation { duration: 180 } }
                        }
                    }
                }
            }

            // Preset library
            ColumnLayout {
                visible: win.page === "presets"
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 13
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "YOUR PRESETS"; color: "#e8e9f3"; font.pixelSize: 19; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    Button { text: "＋  New theme"; onClicked: { repo.createTheme(); edit = clone(repo.draft); page = "editor" }
                        background: Rectangle { radius: 12; color: parent.hovered ? "#55415f83" : "#3a303f62"; border.color: "#788a83ad" }
                        contentItem: Text { text: parent.text; color: "#ece6ff"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 11; rightPadding: 11 } }
                    Button { text: "Import"; onClicked: repo.importTheme(); background: Rectangle { radius: 12; color: parent.hovered ? "#30445d79" : "#25303e59"; border.color: "#5e687f9b" }
                        contentItem: Text { text: parent.text; color: "#d1d9e9"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 12; rightPadding: 12 } }
                    Button { text: "Export"; enabled: visibleCount > 0; onClicked: repo.exportPreset(); background: Rectangle { radius: 12; color: parent.hovered ? "#30445d79" : "#25303e59"; border.color: "#5e687f9b" }
                        contentItem: Text { text: parent.text; color: "#d1d9e9"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 12; rightPadding: 12 } }
                }
                ListView {
                    id: presetList
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 7
                    model: repo.themes
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: presetList.width; height: 54; radius: 14
                        color: repo.selected === index ? "#583c4869" : "#85141c2c"
                        border.color: repo.selected === index ? "#8294afd0" : "#3e353f57"
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 12
                            Row { spacing: 4; Repeater { model: [modelData.palette.accent, modelData.palette.secondary, modelData.palette.tertiary]; delegate: Rectangle { required property string modelData; width: 15; height: 15; radius: 8; color: modelData } } }
                            ColumnLayout { Layout.fillWidth: true; spacing: 1
                                Text { text: modelData.name; color: "#edf0f7"; font.pixelSize: 13; font.weight: Font.Medium }
                                Text { text: modelData.builtin ? "Bundled preset" : "Custom preset"; color: "#9aa5bb"; font.pixelSize: 10 }
                            }
                            Button { text: "Select"; onClicked: { repo.selected = index; page = "gallery" }
                                background: Rectangle { radius: 12; color: parent.hovered ? "#3e4c6880" : "#2432475d"; border.color: "#50617a98" }
                                contentItem: Text { text: parent.text; color: "#d3dced"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 8; rightPadding: 8 } }
                            Button { text: "Duplicate"; onClicked: { repo.selected = index; repo.duplicateSelected(); edit = clone(repo.draft); page = "editor" }
                                background: Rectangle { radius: 12; color: parent.hovered ? "#3e4c6880" : "#2432475d"; border.color: "#50617a98" }
                                contentItem: Text { text: parent.text; color: "#d3dced"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 8; rightPadding: 8 } }
                            Button { text: modelData.builtin ? "Edit copy" : "Edit"; onClicked: { repo.selected = index; win.beginEdit() }
                                background: Rectangle { radius: 12; color: parent.hovered ? "#3e4c6880" : "#2432475d"; border.color: "#50617a98" }
                                contentItem: Text { text: parent.text; color: "#d3dced"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 8; rightPadding: 8 } }
                            Button { text: "Delete"; enabled: !modelData.builtin; onClicked: { repo.selected = index; repo.deleteSelected() }
                                background: Rectangle { radius: 12; color: parent.hovered ? "#563a415b" : "#322b293a"; border.color: "#685d4665" }
                                contentItem: Text { text: parent.text; color: parent.enabled ? "#e3b4c0" : "#827b89"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 8; rightPadding: 8 } }
                        }
                    }
                }
                Text { text: "Preset changes are stored in your Aurora Studio library."; color: "#8e9ab0"; font.pixelSize: 11 }
            }

            // Theme editor, retained with its existing save, discard, and palette workflows.
            ColumnLayout {
                visible: win.page === "editor"
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 13
                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "‹  Gallery"; onClicked: { pendingPage = "gallery"; win.cancelEdit() }
                        background: Rectangle { radius: 12; color: parent.hovered ? "#31425c78" : "#24303e59" }
                        contentItem: Text { text: parent.text; color: "#d3dced"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 11; rightPadding: 11 } }
                    Item { Layout.fillWidth: true }
                    Text { text: "CUSTOM THEME EDITOR"; color: "#b0bad0"; font.pixelSize: 10; font.letterSpacing: 1.8 }
                    Item { Layout.fillWidth: true }
                    Button { text: "Cancel"; onClicked: win.cancelEdit(); background: Rectangle { radius: 12; color: parent.hovered ? "#31425c78" : "#24303e59" }
                        contentItem: Text { text: parent.text; color: "#d3dced"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 12; rightPadding: 12 } }
                    Button { text: "Save preset"; onClicked: repo.saveDraft(edit)
                        background: Rectangle {
                            radius: 12
                            gradient: Gradient {
                                GradientStop { position: 0; color: parent.hovered ? "#9a78fa" : "#765bd2" }
                                GradientStop { position: 1; color: parent.hovered ? "#56a9e9" : "#397dbb" }
                            }
                        }
                        contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 13; rightPadding: 13 } }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 17
                    Flickable {
                        Layout.preferredWidth: 455; Layout.fillHeight: true; clip: true; contentWidth: width; contentHeight: editorFields.implicitHeight
                        ColumnLayout {
                            id: editorFields; width: parent.width; spacing: 8
                            TextField { Layout.fillWidth: true; placeholderText: "Theme name"; text: edit.name || ""; onTextEdited: { let d = clone(edit); d.name = text; edit = d }
                                background: Rectangle { radius: 12; color: "#a6172032"; border.color: "#63778ba9" }
                                color: "#f1f2f8"; placeholderTextColor: "#939db2"; leftPadding: 13; selectionColor: "#7c65c5" }
                            TextField { Layout.fillWidth: true; placeholderText: "Short description"; text: edit.description || ""; onTextEdited: { let d = clone(edit); d.description = text; edit = d }
                                background: Rectangle { radius: 12; color: "#a6172032"; border.color: "#63778ba9" }
                                color: "#f1f2f8"; placeholderTextColor: "#939db2"; leftPadding: 13; selectionColor: "#7c65c5" }
                            Text { text: "PALETTE COLORS"; color: "#aab5ca"; font.pixelSize: 10; font.letterSpacing: 1.5; topPadding: 6 }
                            Repeater {
                                model: ["background", "surface", "accent", "secondary", "tertiary", "text", "muted", "border", "error", "warning", "success"]
                                delegate: RowLayout {
                                    required property string modelData
                                    Layout.fillWidth: true; spacing: 8
                                    Text { text: modelData; color: "#cbd3e2"; font.pixelSize: 11; Layout.fillWidth: true }
                                    Rectangle { width: 18; height: 18; radius: 9; color: win.color(modelData); border.color: "#98ffffff" }
                                    TextField { Layout.preferredWidth: 94; height: 30; text: win.color(modelData); onTextEdited: win.setColor(modelData, text); validator: RegularExpressionValidator { regularExpression: /^#[0-9A-Fa-f]{6}$/ }
                                        background: Rectangle { radius: 12; color: "#aa111b2e"; border.color: "#566b829f" }
                                        color: "#e1e5ee"; font.pixelSize: 11; leftPadding: 8; selectionColor: "#7c65c5" }
                                    Button { text: "◉"; width: 31; height: 30; padding: 0; onClicked: { picker.targetKey = modelData; picker.currentColor = win.color(modelData); picker.open() }
                                        background: Rectangle { radius: 12; color: parent.hovered ? "#48506b88" : "#29364964"; border.color: "#596e879f" }
                                        contentItem: Text { text: parent.text; color: "#e3ddff"; font.pixelSize: 15; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } }
                                }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true; radius: 20; color: "#8f111a2d"; border.color: "#557b8ca9"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 18; spacing: 12
                            Text { text: "LIVE PREVIEW"; color: "#b6bfd2"; font.pixelSize: 10; font.letterSpacing: 1.8 }
                            Rectangle {
                                Layout.fillWidth: true; height: 42; radius: 13; color: win.color("surface")
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 13; anchors.rightMargin: 13
                                    Text { text: "✦  Aurora"; color: win.color("text"); font.pixelSize: 12 }
                                    Item { Layout.fillWidth: true }
                                    Text { text: "◉"; color: win.color("accent"); font.pixelSize: 17 }
                                }
                            }
                            Text { Layout.fillWidth: true; text: edit.name || "A quiet place to create"; color: win.color("text"); font.pixelSize: 20; font.weight: Font.DemiBold; wrapMode: Text.WordWrap }
                            Text { Layout.fillWidth: true; text: "Colors update here while you edit. Your desktop remains untouched."; color: win.color("muted"); font.pixelSize: 12; wrapMode: Text.WordWrap }
                            Row { spacing: 8; Repeater { model: ["accent", "secondary", "tertiary"]; delegate: Rectangle { required property string modelData; width: 26; height: 26; radius: 13; color: win.color(modelData); border.color: "#70ffffff" } } }
                            Rectangle {
                                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 110; radius: 12; color: win.color("surface")
                                Column {
                                    anchors.fill: parent; anchors.margins: 14; spacing: 7
                                    Text { text: "~/aurora-studio"; color: win.color("muted"); font.family: "monospace"; font.pixelSize: 11 }
                                    Text { text: "❯ echo ‘hello, aurora’"; color: win.color("text"); font.family: "monospace"; font.pixelSize: 12 }
                                    Text { text: "hello, aurora"; color: win.color("accent"); font.family: "monospace"; font.pixelSize: 12 }
                                }
                            }
                        }
                    }
                }
            }

            // Gallery actions remain grouped and centered.
            RowLayout {
                visible: win.page === "gallery"
                Layout.fillWidth: true; Layout.preferredHeight: 46; spacing: 10
                Item { Layout.fillWidth: true }
                Button {
                    text: "Preview"; enabled: visibleCount > 0
                    onClicked: previewDialog.open()
                    background: Rectangle { radius: 12; color: parent.hovered ? "#354a6283" : "#28344867"; border.color: "#66738ca8" }
                    contentItem: Text { text: parent.text; color: "#e0e5f0"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 19; rightPadding: 19 }
                }
                Button {
                    text: "Apply"; enabled: visibleCount > 0 && repo.integrations.length > 0 && repo.integrations[0].success
                    onClicked: applyDialog.open()
                    background: Rectangle { radius: 12; color: parent.enabled ? (parent.hovered ? "#45546f91" : "#35445f7e") : "#262d3d55"; border.color: parent.enabled ? "#7786a5c2" : "#47566e88" }
                    contentItem: Text { text: parent.text; color: parent.enabled ? "#e2e9f7" : "#8b98af"; font.pixelSize: 12; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 24; rightPadding: 24 }
                    ToolTip.visible: hovered; ToolTip.text: "Apply supported Alacritty colors. Other integrations are listed as unsupported."
                }
                Button {
                    visible: repo.hasRestorePoint
                    text: "Restore Previous Theme"
                    onClicked: restoreDialog.open()
                    background: Rectangle { radius: 12; color: parent.hovered ? "#4b394961" : "#2c34475f"; border.color: "#687b91aa" }
                    contentItem: Text { text: parent.text; color: "#d1daeb"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 8; rightPadding: 8; elide: Text.ElideRight }
                }
                Button {
                    text: "Customize"; enabled: visibleCount > 0
                    onClicked: win.beginEdit()
                    background: Rectangle {
                        radius: 12
                        gradient: Gradient {
                            GradientStop { position: 0; color: parent.hovered ? "#a284ff" : "#8064db" }
                            GradientStop { position: 1; color: parent.hovered ? "#68b8f2" : "#428ac5" }
                        }
                        border.color: "#b6c4ecff"
                    }
                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 24; rightPadding: 24 }
                }
                Item { Layout.fillWidth: true }
            }
        }
    }

    Dialog {
        id: previewDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: "Theme preview"
        standardButtons: Dialog.Close
        background: Rectangle { radius: 18; color: "#f21a2438"; border.color: "#8092a8c9" }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: visibleCount ? repo.themes[repo.selected].name : "No preset selected"; color: "#f1f2f8"; font.pixelSize: 19; font.weight: Font.DemiBold }
            Text { text: visibleCount ? repo.themes[repo.selected].description : ""; color: "#c3cada"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.preferredWidth: 360 }
            Row { spacing: 8; Repeater { model: visibleCount ? [repo.themes[repo.selected].palette.background, repo.themes[repo.selected].palette.surface, repo.themes[repo.selected].palette.accent, repo.themes[repo.selected].palette.secondary, repo.themes[repo.selected].palette.tertiary] : []; delegate: Rectangle { required property string modelData; width: 30; height: 30; radius: 15; color: modelData; border.color: "#8cffffff" } } }
            Text { text: "Preview only. No desktop settings change until you confirm Apply."; color: "#9eabc2"; font.pixelSize: 11 }
        }
    }
    Dialog {
        id: applyDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: "Apply supported theme settings?"
        standardButtons: Dialog.NoButton
        background: Rectangle { radius: 18; color: "#f21a2438"; border.color: "#8092a8c9" }
        contentItem: ColumnLayout {
            spacing: 11
            Text { text: "Aurora Studio will update Alacritty's color sections. A byte-for-byte backup is kept for Restore Previous Theme. Changes take effect in new terminal windows."; color: "#dce3f0"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.preferredWidth: 390 }
            Repeater {
                model: repo.integrations
                delegate: Text {
                    required property var modelData
                    Layout.fillWidth: true
                    text: (modelData.supported ? "✓ " : "— ") + modelData.name + ": " + modelData.message
                    color: modelData.supported ? "#cce5d7" : "#9ea9bd"
                    font.pixelSize: 11; wrapMode: Text.WordWrap
                }
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                Button {
                    text: "Cancel"
                    onClicked: applyDialog.close()
                    background: Rectangle { radius: 12; color: "#30435f7a" }
                    contentItem: Text { text: parent.text; color: "#dce2ef"; font.pixelSize: 12; leftPadding: 13; rightPadding: 13; horizontalAlignment: Text.AlignHCenter }
                }
                Button {
                    text: "Apply"
                    onClicked: { applyDialog.close(); repo.applySelectedTheme() }
                    background: Rectangle { radius: 12; color: "#765fba"; border.color: "#a291aee3" }
                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12; font.weight: Font.DemiBold; leftPadding: 15; rightPadding: 15; horizontalAlignment: Text.AlignHCenter }
                }
            }
        }
    }
    Dialog {
        id: restoreDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: "Restore previous configuration?"
        standardButtons: Dialog.NoButton
        background: Rectangle { radius: 18; color: "#f21a2438"; border.color: "#8092a8c9" }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "Restore the saved Alacritty configuration from before Aurora Studio's first Apply?"; color: "#dce3f0"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.preferredWidth: 350 }
            RowLayout {
                Item { Layout.fillWidth: true }
                Button {
                    text: "Cancel"
                    onClicked: restoreDialog.close()
                    background: Rectangle { radius: 12; color: "#30435f7a" }
                    contentItem: Text { text: parent.text; color: "#dce2ef"; font.pixelSize: 12; leftPadding: 13; rightPadding: 13; horizontalAlignment: Text.AlignHCenter }
                }
                Button {
                    text: "Restore"
                    onClicked: { restoreDialog.close(); repo.restorePreviousTheme() }
                    background: Rectangle { radius: 12; color: "#76566eaa" }
                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12; leftPadding: 15; rightPadding: 15; horizontalAlignment: Text.AlignHCenter }
                }
            }
        }
    }
    Dialog {
        id: discardDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: "Discard unsaved changes?"
        standardButtons: Dialog.NoButton
        background: Rectangle { radius: 18; color: "#f21a2438"; border.color: "#8092a8c9" }
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Your editor changes have not been saved."; color: "#e0e5f0"; font.pixelSize: 13 }
            RowLayout {
                Item { Layout.fillWidth: true }
                Button { text: "Keep editing"; onClicked: { discardDialog.close(); pendingPage = ""; pendingClose = false }
                    background: Rectangle { radius: 12; color: "#30435f7a"; border.color: "#5d6d86a3" }
                    contentItem: Text { text: parent.text; color: "#dce2ef"; font.pixelSize: 12; leftPadding: 12; rightPadding: 12; horizontalAlignment: Text.AlignHCenter } }
                Button { text: "Discard"; onClicked: { win.discardEdit(); if (pendingClose) { pendingClose = false; win.close() } else if (pendingPage !== "") { win.page = pendingPage; pendingPage = "" } }
                    background: Rectangle { radius: 12; color: "#76566eaa"; border.color: "#a291aee3" }
                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12; leftPadding: 12; rightPadding: 12; horizontalAlignment: Text.AlignHCenter } }
            }
        }
    }
    ColorDialog { id: picker; property string targetKey: "accent"; onAccepted: win.setColor(targetKey, color.toString()) }
    Connections {
        target: repo
        function onDraftChanged() {
            if (repo.editing) { edit = clone(repo.draft); page = "editor" }
            else if (page === "editor" && !discardDialog.opened) page = "gallery"
        }
        function onThemesChanged() { visibleCount = repo.themes.length }
    }
    Shortcut { sequence: "Left"; enabled: page === "gallery"; onActivated: win.moveSelection(-1) }
    Shortcut { sequence: "Right"; enabled: page === "gallery"; onActivated: win.moveSelection(1) }
    Shortcut { sequence: "Return"; enabled: page === "gallery" && visibleCount > 0; onActivated: win.beginEdit() }
    Shortcut { sequence: "Ctrl+N"; enabled: page !== "editor"; onActivated: { repo.createTheme(); edit = clone(repo.draft); page = "editor" } }
    Shortcut { sequence: "Escape"; onActivated: { if (picker.visible) picker.close(); else if (discardDialog.opened) { discardDialog.close(); pendingPage = ""; pendingClose = false } else if (page === "editor") win.cancelEdit(); else win.close() } }
    Rectangle {
        id: toast
        property string message: ""
        visible: message !== ""
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 15
        width: Math.min(440, toastLabel.implicitWidth + 32); height: 40; radius: 12; z: 100
        color: "#ed29374f"; border.color: "#748398b4"
        Text { id: toastLabel; anchors.centerIn: parent; text: toast.message; color: "#f0f2f7"; font.pixelSize: 12 }
        Timer { id: toastTimer; interval: 3200; onTriggered: toast.message = "" }
    }
}
