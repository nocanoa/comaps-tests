#!/usr/bin/env python3
#
# Check AppStore/GooglePlay metadata
#

import os
import sys
import glob
import shutil
from urllib.parse import urlparse

os.chdir(os.path.join(os.path.dirname(os.path.realpath(__file__)), "..", ".."))

# https://support.google.com/googleplay/android-developer/answer/9844778?visit_id=637740303439369859-3116807078&rd=1#zippy=%2Cview-list-of-available-languages
GPLAY_LOCALES = [
    "af",    # Afrikaans
    "sq",    # Albanian
    "am",    # Amharic
    "ar",    # Arabic
    "hy-AM", # Armenian
    "az-AZ", # Azerbaijani
    "bn-BD", # Bangla
    "eu-ES", # Basque
    "be",    # Belarusian
    "bg",    # Bulgarian
    "my-MM", # Burmese
    "ca",    # Catalan
    "zh-HK", # Chinese (Hong Kong)
    "zh-CN", # Chinese (Simplified)
    "zh-TW", # Chinese (Traditional)
    "hr",    # Croatian
    "cs-CZ", # Czech
    "da-DK", # Danish
    "nl-NL", # Dutch
    "en-IN", # English
    "en-SG", # English
    "en-ZA", # English
    "en-AU", # English (Australia)
    "en-CA", # English (Canada)
    "en-GB", # English (United Kingdom)
    "en-US", # English (United States)
    "et",    # Estonian
    "fil",   # Filipino
    "fi-FI", # Finnish
    "fr-CA", # French (Canada)
    "fr-FR", # French (France)
    "gl-ES", # Galician
    "ka-GE", # Georgian
    "de-DE", # German
    "el-GR", # Greek
    "gu",    # Gujarati
    "iw-IL", # Hebrew
    "hi-IN", # Hindi
    "hu-HU", # Hungarian
    "is-IS", # Icelandic
    "id",    # Indonesian
    "it-IT", # Italian
    "ja-JP", # Japanese
    "kn-IN", # Kannada
    "kk",    # Kazakh
    "km-KH", # Khmer
    "ko-KR", # Korean
    "ky-KG", # Kyrgyz
    "lo-LA", # Lao
    "lv",    # Latvian
    "lt",    # Lithuanian
    "mk-MK", # Macedonian
    "ms",    # Malay
    "ms-MY", # Malay (Malaysia)
    "ml-IN", # Malayalam
    "mr-IN", # Marathi
    "mn-MN", # Mongolian
    "ne-NP", # Nepali
    "no-NO", # Norwegian
    "fa",    # Persian
    "fa-AE", # Persian
    "fa-AF", # Persian
    "fa-IR", # Persian
    "pl-PL", # Polish
    "pt-BR", # Portuguese (Brazil)
    "pt-PT", # Portuguese (Portugal)
    "pa",    # Punjabi
    "ro",    # Romanian
    "rm",    # Romansh
    "ru-RU", # Russian
    "sr",    # Serbian
    "si-LK", # Sinhala
    "sk",    # Slovak
    "sl",    # Slovenian
    "es-419", # Spanish (Latin America)
    "es-ES", # Spanish (Spain)
    "es-US", # Spanish (United States)
    "sw",    # Swahili
    "sv-SE", # Swedish
    "ta-IN", # Tamil
    "te-IN", # Telugu
    "th",    # Thai
    "tr-TR", # Turkish
    "uk",    # Ukrainian
    "ur",    # Urdu
    "vi",    # Vietnamese
    "zu",    # Zulu
]

# From a Fastline error message and https://help.apple.com/app-store-connect/#/dev997f9cf7c
APPSTORE_LOCALES = [
    "ar-SA", "ca", "cs", "da", "de-DE", "el", "en-AU", "en-CA", "en-GB", "en-US", "es-ES", "es-MX", "fi", "fr-CA", "fr-FR", "he", "hi", "hr", "hu", "id", "it", "ja", "ko", "ms", "nl-NL", "no", "pl", "pt-BR", "pt-PT", "ro", "ru", "sk", "sv", "th", "tr", "uk", "vi", "zh-Hans", "zh-Hant"
]

def error(path, message, *args, **kwargs):
    print("❌", path + ":", message.format(*args, **kwargs), file=sys.stderr)
    return False


def done(path, ok):
    if ok:
        print("✅", path)
    return ok

def check_raw(path, max_length, ignoreEmptyFilesAndNewLines=False):
    ok = True
    with open(path, 'r') as f:
        text = f.read()
        if not ignoreEmptyFilesAndNewLines:
            if not text:
                ok = error(path, "empty")
            elif text[-1] == os.linesep:
                text = text[:-1]
            else:
                ok = error(path, "missing new line")
        
        cur_length = len(text)
        if cur_length > max_length:
            ok = error(path, "too long: got={}, expected={}", cur_length, max_length)
        return ok, text

def check_text(path, max, optional=False, ignoreEmptyFilesAndNewLines=False):
    try:
        return done(path, check_raw(path, max, ignoreEmptyFilesAndNewLines)[0])
    except FileNotFoundError as e:
        if optional:
            return True,
        print("🚫", path)
        return False,

def check_url(path, ignoreEmptyFilesAndNewLines=False):
    (ok, url) = check_raw(path, 500, ignoreEmptyFilesAndNewLines)
    url = urlparse(url)
    if not url.scheme in ('https', 'http'):
        ok = error(path, "invalid URL: {}", url)
    return done(path, ok)

def check_email(path):
    (ok, email) = check_raw(path, 500)
    ok = ok and email.find('@') != -1 and email.find('.') != -1
    return done(path, ok)

def check_exact(path, expected):
    (ok, value) = check_raw(path, len(expected))
    if value != expected:
        ok = error(path, "invalid value: got={}, expected={}", value, expected)
    return done(path, ok)


def check_android():
    ok = True
    flavor = 'android/app/src/fdroid/play/'
    ok = check_url(flavor + 'contact-website.txt') and ok
    ok = check_email(flavor + 'contact-email.txt') and ok
    ok = check_exact(flavor + 'default-language.txt', 'en-US') and ok
    for locale in glob.glob(flavor + 'listings/*/'):
        if locale.split('/')[-2] not in GPLAY_LOCALES:
            ok = error(locale, 'unsupported locale') and ok
            continue
        ok = check_text(locale + 'title.txt', 50) and ok
        ok = check_text(locale + 'title-google.txt', 30) and ok
        ok = check_text(locale + 'short-description.txt', 80) and ok
        ok = check_text(locale + 'short-description-google.txt', 80, True) and ok
        ok = check_text(locale + 'full-description.txt', 4000) and ok
        ok = check_text(locale + 'full-description-google.txt', 4000, True) and ok
        ok = check_text(locale + 'release-notes.txt', 499) and ok
    for locale in glob.glob(flavor + 'release-notes/*/'):
        if locale.split('/')[-2] not in GPLAY_LOCALES:
            ok = error(locale, 'unsupported locale') and ok
            continue
        ok = check_text(locale + 'default.txt', 499) and ok
    return ok


def check_ios():
    ok = True
    for locale in glob.glob('iphone/metadata/*/'):
        if locale.split('/')[-2] not in APPSTORE_LOCALES:
            ok = error(locale, "unsupported locale") and ok
            continue
        
        locale_complete = True
        for name in ["description.txt", "keywords.txt", "marketing_url.txt", "privacy_url.txt", "subtitle.txt", "support_url.txt"]:
            name_path = os.path.join(locale, name)
            if not os.path.exists(name_path):
                locale_complete = False
        
        if locale_complete:
            ok = check_text(locale + "subtitle.txt", 30, False, True) and ok
            ok = check_text(locale + "description.txt", 4000, False, True) and ok
            ok = check_text(locale + "release_notes.txt", 4000, True, True) and ok
            ok = check_text(locale + "keywords.txt", 100, False, True) and ok
            ok = check_url(locale + "support_url.txt", True) and ok
            ok = check_url(locale + "marketing_url.txt", True) and ok
            ok = check_url(locale + "privacy_url.txt", True) and ok
        
    return ok

if __name__ == "__main__":
    ok = True
    if len(sys.argv) == 2 and sys.argv[1] == 'android':
        if check_android():
            sys.exit(0)
        sys.exit(2)
    elif len(sys.argv) == 2 and sys.argv[1] == "ios":
        if check_ios():
            sys.exit(0)
        sys.exit(2)
    else:
       print("Usage:", sys.argv[0], "android|ios", file=sys.stderr)  
       sys.exit(1)
