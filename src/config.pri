# Enable handwriting
handwriting:!lipi-toolkit:!t9write:!myscript {
    include(plugins/myscript/3rdparty/myscript/myscript.pri)
    equals(MYSCRIPT_FOUND, 1) {
        CONFIG += myscript
    } else {
        include(plugins/t9write/3rdparty/t9write/t9write-build.pri)
        equals(T9WRITE_FOUND, 1): CONFIG += t9write
        else: CONFIG += lipi-toolkit
    }
}
myscript {
    !handwriting: include(plugins/myscript/3rdparty/myscript/myscript.pri)
}
t9write {
    !handwriting: include(plugins/t9write/3rdparty/t9write/t9write-build.pri)
    equals(T9WRITE_CJK_FOUND, 1): CONFIG += t9write-cjk
    equals(T9WRITE_ALPHABETIC_FOUND, 1): CONFIG += t9write-alphabetic
}

# Enable pkgconfig
win32: CONFIG += no-pkg-config
!no-pkg-config: CONFIG += link_pkgconfig

# Enable Hunspell
!disable-hunspell:!hunspell-library:!hunspell-package {
    exists(plugins/hunspell/3rdparty/hunspell/src/hunspell/hunspell.h): CONFIG += hunspell-library
    else:link_pkgconfig:packagesExist(hunspell): CONFIG += hunspell-package
    else: CONFIG += disable-hunspell
}
disable-hunspell: CONFIG -= hunspell
else: CONFIG += hunspell

# Disable built-in layouts
disable-layouts {
    message("The built-in layouts are now excluded from the Qt Virtual Keyboard plugin.")
} else {
    # Enable languages by features
    openwnn: CONFIG += lang-ja_JP
    hangul: CONFIG += lang-ko_KR
    pinyin: CONFIG += lang-zh_CN
    tcime|zhuyin|cangjie: CONFIG += lang-zh_TW

    # Use all languages by default
    !contains(CONFIG, lang-.*): CONFIG += lang-all

    # Flag for activating all languages
    lang-all: CONFIG += \
        lang-ar_AR \
        lang-bg_BG \
        lang-cs_CZ \
        lang-da_DK \
        lang-de_DE \
        lang-el_GR \
        lang-en_GB \
        lang-es_ES \
        lang-et_EE \
        lang-fa_FA \
        lang-fi_FI \
        lang-fr_FR \
        lang-he_IL \
        lang-hi_IN \
        lang-hr_HR \
        lang-hu_HU \
        lang-it_IT \
        lang-ja_JP \
        lang-ko_KR \
        lang-nb_NO \
        lang-nl_NL \
        lang-pl_PL \
        lang-pt_PT \
        lang-ro_RO \
        lang-ru_RU \
        lang-sk_SK \
        lang-sl_SI \
        lang-sq_AL \
        lang-sr_SP \
        lang-sv_SE \
        lang-vi_VN \
        lang-zh_CN \
        lang-zh_TW
}

# Common variables
LAYOUTS_BASE = $$PWD/virtualkeyboard
LAYOUTS_PREFIX = /QtQuick/VirtualKeyboard
VIRTUALKEYBOARD_INSTALL_DATA = $$[QT_INSTALL_DATA]/qtvirtualkeyboard

# Enable features by languages
contains(CONFIG, lang-ja.*)|lang-all: CONFIG += openwnn
contains(CONFIG, lang-ko.*)|lang-all: CONFIG += hangul
contains(CONFIG, lang-zh(_CN)?)|lang-all: CONFIG += pinyin
contains(CONFIG, lang-zh(_TW)?)|lang-all: CONFIG += tcime

# Feature dependencies
tcime {
    !cangjie:!zhuyin: CONFIG += cangjie zhuyin
} else {
    cangjie|zhuyin: CONFIG += tcime
}

# Deprecated configuration flags
disable-xcb {
    message("The disable-xcb option has been deprecated. Please use disable-desktop instead.")
    CONFIG += disable-desktop
}
