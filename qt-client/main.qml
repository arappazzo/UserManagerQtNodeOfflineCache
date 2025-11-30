import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.15

Window {
    width: 640
    height: 500
    visible: true
    title: "User Manager"

    Material.theme: Material.Light
    Material.accent: Material.Blue

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 0

            ListView {
                id: userList
                anchors.fill: parent
                model: _dbUserModel
                clip: true

                delegate: Rectangle {
                    height: 50
                    width: ListView.view.width
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 16

                        Label {
                            text: Name
                            Layout.fillWidth: true
                            font.pixelSize: 16
                        }

                        Label {
                            text: Age
                            Layout.preferredWidth: 60
                            font.pixelSize: 16
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            text: "Remove"
                            Layout.preferredWidth: 90
                            onClicked: _dbUserModel.deleteUserFromServer(TableId)
                        }
                    }

                    Rectangle {
                        height: 1
                        width: parent.width
                        color: "#cccccc"
                    }
                }

                ScrollBar.vertical: ScrollBar { }
            }
        }

        // -------- FORM DI INPUT --------
        Frame {
            Layout.fillWidth: true
            height: 60
            padding: 12

            RowLayout {
                anchors.fill: parent
                spacing: 12

                TextField {
                    id: nameInput
                    placeholderText: "Name"
                    Layout.fillWidth: true
                }

                TextField {
                    id: ageInput
                    placeholderText: "Age"
                    Layout.preferredWidth: 70
                    inputMethodHints: Qt.ImhDigitsOnly
                }

                Button {
                    text: "Add"
                    Layout.preferredWidth: 80

                    onClicked: {
                        if (nameInput.text === "" || ageInput.text === "")
                            return

                        _dbUserModel.sendUserToServer(
                            nameInput.text,
                            parseInt(ageInput.text)
                        )

                        nameInput.text = ""
                        ageInput.text = ""
                    }
                }
            }
        }
    }
}
