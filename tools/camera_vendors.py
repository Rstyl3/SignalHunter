"""Curated vendor list for gen_camera_oui.py.

Each entry: (regex matched case-insensitively against the IEEE organisation
name, display name shown on the LCD (keep it short), tier).

CAM = camera-focused vendor: a match is a strong hint.
IOT = generic chipset / IoT / consumer-electronics vendor whose blocks are
      also used by countless cheap cameras: a match is a weak hint.

Add a vendor here and re-run tools/gen_camera_oui.py; never edit the generated
header by hand.
"""

CAM = "CAM"
IOT = "IOT"

VENDORS = [
    # --- camera-focused vendors ---------------------------------------------
    (r"hikvision",                                    "Hikvision",  CAM),
    (r"\bezviz\b",                                    "Ezviz",      CAM),
    (r"zhejiang dahua|dahua technology",              "Dahua",      CAM),
    (r"\bimou\b",                                     "Imou",       CAM),
    (r"\blorex\b",                                    "Lorex",      CAM),
    (r"zhejiang uniview|uniview technolog",           "Uniview",    CAM),
    (r"\btiandy\b",                                   "Tiandy",     CAM),
    (r"\bkedacom\b",                                  "Kedacom",    CAM),
    (r"wyze labs",                                    "Wyze",       CAM),
    (r"hualai technology",                            "Hualai",     CAM),
    (r"\breolink\b",                                  "Reolink",    CAM),
    (r"\bamcrest\b",                                  "Amcrest",    CAM),
    (r"\bfoscam\b",                                   "Foscam",     CAM),
    (r"axis communications",                          "Axis",       CAM),
    (r"hanwha (techwin|vision)|samsung techwin",      "Hanwha",     CAM),
    (r"\bvivotek\b",                                  "Vivotek",    CAM),
    (r"\bmobotix\b",                                  "Mobotix",    CAM),
    (r"bosch security|bosch sicherheits",             "Bosch",      CAM),
    (r"\bavigilon\b",                                 "Avigilon",   CAM),
    (r"\bpelco\b",                                    "Pelco",      CAM),
    (r"\bflir\b",                                     "FLIR",       CAM),
    (r"\bgeovision\b",                                "GeoVision",  CAM),
    (r"acti corporation",                             "ACTi",       CAM),
    (r"\bmilesight\b",                                "Milesight",  CAM),
    (r"provision[- ]isr",                             "Provision",  CAM),
    (r"\bverkada\b",                                  "Verkada",    CAM),
    (r"arlo technolog",                               "Arlo",       CAM),
    (r"^ring llc|^ring inc|ring,? llc",               "Ring",       CAM),
    (r"immedia semiconductor",                        "Blink",      CAM),
    (r"anker innovations|\beufy\b",                   "eufy",       CAM),
    (r"nest labs",                                    "Nest",       CAM),
    (r"shanghai xiaoyi|xiaoyi technology|kami vision", "Yi",        CAM),
    (r"shanghai imilab|\bimilab\b",                   "Imilab",     CAM),
    (r"\bvstarcam\b",                                 "VStarcam",   CAM),
    (r"\bxiongmai\b",                                 "Xiongmai",   CAM),
    (r"\bannke\b",                                    "Annke",      CAM),
    (r"swann communications",                         "Swann",      CAM),
    (r"\bzmodo\b",                                    "Zmodo",      CAM),
    (r"\bwansview\b",                                 "Wansview",   CAM),
    (r"\bsricam\b",                                   "Sricam",     CAM),
    (r"\bjooan\b",                                    "Jooan",      CAM),
    (r"\bdericam\b",                                  "Dericam",    CAM),
    (r"\btenvis\b",                                   "Tenvis",     CAM),
    (r"\blongse\b",                                   "Longse",     CAM),
    (r"\btvt digital\b|shenzhen tvt",                 "TVT",        CAM),
    (r"\bhichip\b",                                   "HiChip",     CAM),
    (r"shenzhen reecam|\breecam\b",                   "Reecam",     CAM),
    (r"shenzhen gwell|\bgwell\b",                     "Yoosee",     CAM),
    (r"\bsunell\b",                                   "Sunell",     CAM),
    (r"\binfinova\b",                                 "Infinova",   CAM),
    (r"\bgopro\b",                                    "GoPro",      CAM),
    (r"\bnetatmo\b",                                  "Netatmo",    CAM),
    (r"canary connect",                               "Canary",     CAM),
    # --- generic chipset / IoT vendors (weak hint) ----------------------------
    (r"\bespressif\b",                                "Espressif",  IOT),
    (r"\brealtek\b",                                  "Realtek",    IOT),
    (r"\bmediatek\b|\bralink\b",                      "MediaTek",   IOT),
    (r"tuya smart|hangzhou tuya|\btuya\b",            "Tuya",       IOT),
    (r"\bxiaomi\b",                                   "Xiaomi",     IOT),
    (r"tp-?link",                                     "TP-Link",    IOT),
    (r"amazon technologies",                          "Amazon",     IOT),
    (r"^google,? (inc|llc)|^google llc",              "Google",     IOT),
    (r"d-link",                                       "D-Link",     IOT),
    (r"shenzhen tenda|tenda technology",              "Tenda",      IOT),
    (r"\bubiquiti\b",                                 "Ubiquiti",   IOT),
    (r"ai-thinker|shenzhen anxinke",                  "Ai-Thinker", IOT),
    (r"shenzhen bilian|bilian electronic",            "Bilian",     IOT),
    (r"\bitead\b",                                    "ITEAD",      IOT),
]
