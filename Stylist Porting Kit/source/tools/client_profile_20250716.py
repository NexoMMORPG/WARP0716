"""Native ABI contracts for the supported 2025-07-16 x86 client."""
# Native Stylist by ICE & SCIEL
# SPDX-License-Identifier: GPL-3.0-or-later
GUARDS = {12666878: 'e8fd32e9ff', 12666916: 'e8d732e9ff', 13230748: '0fbf8540bbffff', 8048237: 'e8be2e5d00', 14378651: '0081db00', 7494496: '558bec6aff', 7973839: '0fbf4268', 7973860: '0fbf426a', 7983858: '0fbf4768', 7983873: '0fbf476a'}

EXTRA_HOOKS = [(12666832, 'PacketRead', '558bec83ec10'), (11806080, 'BodyPaletteName', '558bec6aff'), (11808176, 'HairPaletteName', '558bec6aff'), (13884658, 'WorldPaletteRace', '6a005053ff7648'), (8227356, 'RenderPaletteRace', '6a00ffb54cffffff'), (11804048, 'CostumeHeadPath', '558bec6aff'), (11813616, 'CostumeRobePath', '558bec6aff'), (5715792, 'CostumeFileOpen', '558bec6aff'), (5714544, 'CostumeExists', '558bec6aff'), (7504592, 'CostumeSpriteLoad', '558bec6aff'), (5662576, 'CostumePaletteConvert', '558bec518b4510'), (5716352, 'CostumeRead', '558bec5156'), (8236848, 'CostumeRobeMetadata', '558bec6aff'), (8220192, 'CostumeRobeOrder', '558bec6aff'), (14029296, 'CostumeRobeInfo', '558bec83ec0c'), (13856096, 'CostumeRobeDrawOnTop', '558bec6aff')]

HAIR_SITES = [(11808447, True), (11808488, True), (11809174, False), (11809218, False), (11810098, True), (11810143, True), (11810205, False), (11810248, False)]
