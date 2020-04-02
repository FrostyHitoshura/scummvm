/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "ultima/ultima8/misc/pent_include.h"
#include "ultima/ultima8/misc/util.h"
#include "ultima/ultima8/games/game_data.h"
#include "ultima/ultima8/filesys/file_system.h"
#include "ultima/ultima8/filesys/idata_source.h"
#include "ultima/ultima8/usecode/usecode_flex.h"
#include "ultima/ultima8/graphics/main_shape_archive.h"
#include "ultima/ultima8/graphics/fonts/font_shape_archive.h"
#include "ultima/ultima8/graphics/gump_shape_archive.h"
#include "ultima/ultima8/filesys/raw_archive.h"
#include "ultima/ultima8/world/map_glob.h"
#include "ultima/ultima8/graphics/palette_manager.h"
#include "ultima/ultima8/graphics/shape.h"
#include "ultima/ultima8/graphics/wpn_ovlay_dat.h"
#include "ultima/ultima8/kernel/core_app.h"
#include "ultima/ultima8/conf/config_file_manager.h"
#include "ultima/ultima8/graphics/fonts/font_manager.h"
#include "ultima/ultima8/games/game_info.h"
#include "ultima/ultima8/conf/setting_manager.h"
#include "ultima/ultima8/convert/crusader/convert_shape_crusader.h"
#include "ultima/ultima8/audio/music_flex.h"
#include "ultima/ultima8/audio/sound_flex.h"
#include "ultima/ultima8/audio/speech_flex.h"

namespace Ultima {
namespace Ultima8 {

GameData *GameData::_gameData = nullptr;

GameData::GameData(GameInfo *gameInfo)
	: _fixed(nullptr), _mainShapes(nullptr), _mainUsecode(nullptr), _globs(),
	  _fonts(nullptr), _gumps(nullptr), _mouse(nullptr), _music(nullptr),
	  _weaponOverlay(nullptr), _soundFlex(nullptr), _gameInfo(gameInfo) {
	debugN(MM_INFO, "Creating GameData...\n");

	_gameData = this;
	_speech.resize(1024);
}

GameData::~GameData() {
	debugN(MM_INFO, "Destroying GameData...\n");

	delete _fixed;
	_fixed = nullptr;

	delete _mainShapes;
	_mainShapes = nullptr;

	delete _mainUsecode;
	_mainUsecode = nullptr;

	for (unsigned int i = 0; i < _globs.size(); ++i)
		delete _globs[i];
	_globs.clear();

	delete _fonts;
	_fonts = nullptr;

	delete _gumps;
	_gumps = nullptr;

	delete _mouse;
	_mouse = nullptr;

	delete _music;
	_music = nullptr;

	delete _weaponOverlay;
	_weaponOverlay = nullptr;

	delete _soundFlex;
	_soundFlex = nullptr;

	_gameData = nullptr;

	for (unsigned int i = 0; i < _speech.size(); ++i) {
		SpeechFlex **s = _speech[i];
		if (s) delete *s;
		delete s;
	}
	_speech.clear();
}

MapGlob *GameData::getGlob(uint32 glob) const {
	if (glob < _globs.size())
		return _globs[glob];
	else
		return nullptr;
}

ShapeArchive *GameData::getShapeFlex(uint16 flexId) const {
	switch (flexId) {
	case MAINSHAPES:
		return _mainShapes;
	case GUMPS:
		return _gumps;
	default:
		break;
	};
	return nullptr;
}

Shape *GameData::getShape(FrameID f) const {
	ShapeArchive *sf = getShapeFlex(f._flexId);
	if (!sf)
		return nullptr;
	Shape *shape = sf->getShape(f._shapeNum);
	return shape;
}

const ShapeFrame *GameData::getFrame(FrameID f) const {
	const Shape *shape = getShape(f);
	if (!shape)
		return nullptr;
	const ShapeFrame *frame = shape->getFrame(f._frameNum);
	return frame;
}

void GameData::loadTranslation() {
	ConfigFileManager *config = ConfigFileManager::get_instance();
	Std::string translationfile;

	if (_gameInfo->_type == GameInfo::GAME_U8) {
		switch (_gameInfo->_language) {
		case GameInfo::GAMELANG_ENGLISH:
			// default. Don't need to do anything
			break;
		case GameInfo::GAMELANG_FRENCH:
			translationfile = "u8french.ini";
			break;
		case GameInfo::GAMELANG_GERMAN:
			translationfile = "u8german.ini";
			break;
		case GameInfo::GAMELANG_SPANISH:
			translationfile = "u8spanish.ini";
			break;
		case GameInfo::GAMELANG_JAPANESE:
			translationfile = "u8japanese.ini";
			break;
		default:
			perr << "Unknown language." << Std::endl;
			break;
		}
	}

	if (!translationfile.empty()) {
		translationfile = "@data/" + translationfile;

		pout << "Loading translation: " << translationfile << Std::endl;

		config->readConfigFile(translationfile, "language", true);
	}
}

Std::string GameData::translate(Std::string text) {
	// TODO: maybe cache these lookups? config calls may be expensive

	ConfigFileManager *config = ConfigFileManager::get_instance();
	istring key = "language/text/" + text;
	if (!config->exists(key))
		return text;

	Std::string trans;
	config->get(key, trans);
	return trans;
}

FrameID GameData::translate(FrameID f) {
	// TODO: maybe cache these lookups? config calls may be expensive
	// TODO: add any non-gump shapes when applicable
	// TODO: allow translations to be in another shapeflex

	ConfigFileManager *config = ConfigFileManager::get_instance();
	istring key = "language/";
	switch (f._flexId) {
	case GUMPS:
		key += "_gumps/";
		break;
	default:
		return f;
	}

	char buf[100];
	sprintf(buf, "%d,%d", f._shapeNum, f._frameNum);

	key += buf;
	if (!config->exists(key))
		return f;

	Std::string trans;
	config->get(key, trans);

	FrameID t;
	t._flexId = f._flexId;
	int n = sscanf(trans.c_str(), "%u,%u", &t._shapeNum, &t._frameNum);
	if (n != 2) {
		perr << "Invalid shape translation: " << trans << Std::endl;
		return f;
	}

	return t;
}

void GameData::loadU8Data() {
	FileSystem *filesystem = FileSystem::get_instance();

	IDataSource *fd = filesystem->ReadFile("@game/static/fixed.dat");
	if (!fd)
		error("Unable to load static/fixed.dat");;

	_fixed = new RawArchive(fd);

	char langletter = _gameInfo->getLanguageUsecodeLetter();
	if (!langletter)
		error("Unknown language. Unable to open usecode");;

	Std::string filename = "@game/usecode/";
	filename += langletter;
	filename += "usecode.flx";


	IDataSource *uds = filesystem->ReadFile(filename);
	if (!uds)
		error("Unable to load %s", filename.c_str());

	_mainUsecode = new UsecodeFlex(uds);

	// Load main shapes
	pout << "Load Shapes" << Std::endl;
	IDataSource *sf = filesystem->ReadFile("@game/static/u8shapes.flx");
	if (!sf) sf = filesystem->ReadFile("@game/static/u8shapes.cmp");

	if (!sf)
		error("Unable to load static/u8shapes.flx or static/u8shapes.cmp");;

	_mainShapes = new MainShapeArchive(sf, MAINSHAPES,
	                                  PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game));

	// Load weapon, armour info
	ConfigFileManager *config = ConfigFileManager::get_instance();
	config->readConfigFile("@data/u8weapons.ini", "weapons", true);
	config->readConfigFile("@data/u8armour.ini", "armour", true);
	config->readConfigFile("@data/u8monsters.ini", "monsters", true);
	config->readConfigFile("@data/u8.ini", "game", true);

	// Load typeflags
	IDataSource *tfs = filesystem->ReadFile("@game/static/typeflag.dat");
	if (!tfs)
		error("Unable to load static/typeflag.dat");

	_mainShapes->loadTypeFlags(tfs);
	delete tfs;

	// Load animdat
	IDataSource *af = filesystem->ReadFile("@game/static/anim.dat");
	if (!af)
		error("Unable to load static/anim.dat");

	_mainShapes->loadAnimDat(af);
	delete af;

	// Load weapon overlay data
	IDataSource *wod = filesystem->ReadFile("@game/static/wpnovlay.dat");
	if (!wod)
		error("Unable to load static/wpnovlay.dat");

	RawArchive *overlayflex = new RawArchive(wod);
	_weaponOverlay = new WpnOvlayDat();
	_weaponOverlay->load(overlayflex);
	delete overlayflex;

	// Load _globs
	IDataSource *gds = filesystem->ReadFile("@game/static/glob.flx");
	if (!gds)
		error("Unable to load static/glob.flx");

	RawArchive *globflex = new RawArchive(gds);
	_globs.clear();
	_globs.resize(globflex->getCount());
	for (unsigned int i = 0; i < globflex->getCount(); ++i) {
		MapGlob *glob = 0;
		IDataSource *globds = globflex->get_datasource(i);

		if (globds && globds->getSize()) {
			glob = new MapGlob();
			glob->read(globds);
		}
		delete globds;

		_globs[i] = glob;
	}
	delete globflex;

	// Load fonts
	IDataSource *fds = filesystem->ReadFile("@game/static/u8fonts.flx");
	if (!fds)
		error("Unable to load static/u8fonts.flx");

	_fonts = new FontShapeArchive(fds, OTHER,
	                             PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game));
	_fonts->setHVLeads();

	// Load \mouse
	IDataSource *msds = filesystem->ReadFile("@game/static/u8mouse.shp");
	if (!msds)
		error("Unable to load static/u8mouse.shp");

	_mouse = new Shape(msds, 0);
	_mouse->setPalette(PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game));
	delete msds;

	IDataSource *gumpds = filesystem->ReadFile("@game/static/u8gumps.flx");
	if (!gumpds)
		error("Unable to load static/u8gumps.flx");

	_gumps = new GumpShapeArchive(gumpds, GUMPS,
	                             PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game));

	IDataSource *gumpageds = filesystem->ReadFile("@game/static/gumpage.dat");
	if (!gumpageds)
		error("Unable to load static/gumpage.dat");

	_gumps->loadGumpage(gumpageds);
	delete gumpageds;


	IDataSource *mf = filesystem->ReadFile("@game/sound/music.flx");
	if (!mf)
		error("Unable to load sound/music.flx");

	_music = new MusicFlex(mf);

	IDataSource *sndflx = filesystem->ReadFile("@game/sound/sound.flx");
	if (!sndflx)
		error("Unable to load sound/sound.flx");

	_soundFlex = new SoundFlex(sndflx);

	loadTranslation();
}

void GameData::setupFontOverrides() {
	setupTTFOverrides("game/fontoverride", false);

	if (_gameInfo->_language == GameInfo::GAMELANG_JAPANESE)
		setupJPOverrides();
}

void GameData::setupJPOverrides() {
	SettingManager *settingman = SettingManager::get_instance();
	ConfigFileManager *config = ConfigFileManager::get_instance();
	FontManager *fontmanager = FontManager::get_instance();
	KeyMap jpkeyvals;
	KeyMap::const_iterator iter;

	jpkeyvals = config->listKeyValues("language/jpfonts");
	for (iter = jpkeyvals.begin(); iter != jpkeyvals.end(); ++iter) {
		int fontnum = Std::atoi(iter->_key.c_str());
		Std::string fontdesc = iter->_value;

		Std::vector<Std::string> vals;
		SplitString(fontdesc, ',', vals);
		if (vals.size() != 2) {
			perr << "Invalid jpfont override: " << fontdesc << Std::endl;
			continue;
		}

		unsigned int jpfontnum = Std::atoi(vals[0].c_str());
		uint32 col32 = Std::strtol(vals[1].c_str(), 0, 0);

		if (!fontmanager->addJPOverride(fontnum, jpfontnum, col32)) {
			perr << "failed to setup jpfont override for font " << fontnum
			     << Std::endl;
		}
	}

	bool ttfoverrides = false;
	settingman->get("ttf", ttfoverrides);
	if (ttfoverrides)
		setupTTFOverrides("language/fontoverride", true);
}

void GameData::setupTTFOverrides(const char *configkey, bool SJIS) {
	ConfigFileManager *config = ConfigFileManager::get_instance();
	SettingManager *settingman = SettingManager::get_instance();
	FontManager *fontmanager = FontManager::get_instance();
	KeyMap ttfkeyvals;
	KeyMap::const_iterator iter;

	bool ttfoverrides = false;
	settingman->get("ttf", ttfoverrides);
	if (!ttfoverrides) return;

	ttfkeyvals = config->listKeyValues(configkey);
	for (iter = ttfkeyvals.begin(); iter != ttfkeyvals.end(); ++iter) {
		int fontnum = Std::atoi(iter->_key.c_str());
		Std::string fontdesc = iter->_value;

		Std::vector<Std::string> vals;
		SplitString(fontdesc, ',', vals);
		if (vals.size() != 4) {
			perr << "Invalid ttf override: " << fontdesc << Std::endl;
			continue;
		}

		Std::string filename = vals[0];
		int pointsize = Std::atoi(vals[1].c_str());
		uint32 col32 = Std::strtol(vals[2].c_str(), 0, 0);
		int border = Std::atoi(vals[3].c_str());

		if (!fontmanager->addTTFOverride(fontnum, filename, pointsize,
		                                 col32, border, SJIS)) {
			perr << "failed to setup ttf override for font " << fontnum
			     << Std::endl;
		}
	}
}

SpeechFlex *GameData::getSpeechFlex(uint32 shapeNum) {
	if (shapeNum >= _speech.size())
		return nullptr;

	SpeechFlex **s = _speech[shapeNum];
	if (s) return *s;

	s = new SpeechFlex*;
	*s = nullptr;

	FileSystem *filesystem = FileSystem::get_instance();

	Std::string u8_sound_ = "@game/sound/";
	char num_flx [32];
	snprintf(num_flx , 32, "%i.flx", shapeNum);

	char langletter = _gameInfo->getLanguageFileLetter();
	if (!langletter) {
		perr << "GameData::getSpeechFlex: Unknown language." << Std::endl;
		// FIXME: This leaks s
		return nullptr;
	}

	IDataSource *sflx = filesystem->ReadFile(u8_sound_ + langletter + num_flx);
	if (sflx) {
		*s = new SpeechFlex(sflx);
	}

	_speech[shapeNum] = s;

	return *s;
}


void GameData::loadRemorseData() {
	FileSystem *filesystem = FileSystem::get_instance();

	IDataSource *fd = filesystem->ReadFile("@game/static/_fixed.dat");
	if (!fd)
		error("Unable to load static/_fixed.dat");

	_fixed = new RawArchive(fd);

	char langletter = _gameInfo->getLanguageUsecodeLetter();
	if (!langletter)
		error("Unknown language. Unable to open usecode");

	Std::string filename = "@game/usecode/";
	filename += langletter;
	filename += "usecode.flx";


	IDataSource *uds = filesystem->ReadFile(filename);
	if (!uds)
		error("Unable to load %s", filename.c_str());

	_mainUsecode = new UsecodeFlex(uds);

	// Load main shapes
	pout << "Load Shapes" << Std::endl;
	IDataSource *sf = filesystem->ReadFile("@game/static/shapes.flx");

	if (!sf)
		error("Unable to load static/shapes.flx");

	_mainShapes = new MainShapeArchive(sf, MAINSHAPES,
	                                  PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game),
	                                  &CrusaderShapeFormat);

	ConfigFileManager *config = ConfigFileManager::get_instance();
#if 0
	// Load weapon, armour info
	config->readConfigFile("@data/u8weapons.ini", "weapons", true);
	config->readConfigFile("@data/u8armour.ini", "armour", true);
	config->readConfigFile("@data/u8monsters.ini", "monsters", true);
#endif
	config->readConfigFile("@data/remorse.ini", "game", true);

	// Load typeflags
	IDataSource *tfs = filesystem->ReadFile("@game/static/typeflag.dat");
	if (!tfs)
		error("Unable to load static/typeflag.dat");

	_mainShapes->loadTypeFlags(tfs);
	delete tfs;

	// Load animdat
	IDataSource *af = filesystem->ReadFile("@game/static/anim.dat");
	if (!af)
		error("Unable to load static/anim.dat");

	_mainShapes->loadAnimDat(af);
	delete af;

	// Load weapon overlay data
	IDataSource *wod = filesystem->ReadFile("@game/static/wpnovlay.dat");
	if (!wod)
		error("Unable to load static/wpnovlay.dat");

	RawArchive *overlayflex = new RawArchive(wod);
	_weaponOverlay = new WpnOvlayDat();
	_weaponOverlay->load(overlayflex);
	delete overlayflex;

	// Load globs
	IDataSource *gds = filesystem->ReadFile("@game/static/glob.flx");
	if (!gds)
		error("Unable to load static/glob.flx");

	RawArchive *globflex = new RawArchive(gds);
	_globs.clear();
	_globs.resize(globflex->getCount());
	for (unsigned int i = 0; i < globflex->getCount(); ++i) {
		MapGlob *glob = 0;
		IDataSource *globds = globflex->get_datasource(i);

		if (globds && globds->getSize()) {
			glob = new MapGlob();
			glob->read(globds);
		}
		delete globds;

		_globs[i] = glob;
	}
	delete globflex;

	// Load fonts
	IDataSource *fds = filesystem->ReadFile("@game/static/_fonts.flx");
	if (!fds)
		error("Unable to load static/_fonts.flx");

	_fonts = new FontShapeArchive(fds, OTHER,
	                             PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game));
	_fonts->setHVLeads();

	// Load mouse
	IDataSource *msds = filesystem->ReadFile("@game/static/_mouse.shp");
	if (!msds)
		error("Unable to load static/_mouse.shp");

	_mouse = new Shape(msds, 0);
	_mouse->setPalette(PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game));
	delete msds;

	IDataSource *gumpds = filesystem->ReadFile("@game/static/_gumps.flx");
	if (!gumpds)
		error("Unable to load static/_gumps.flx");

	_gumps = new GumpShapeArchive(gumpds, GUMPS,
		PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game));

#if 0
	IDataSource *gumpageds = filesystem->ReadFile("@game/static/gumpage.dat");
	if (!gumpageds)
		error("Unable to load static/gumpage.dat");

	_gumps->loadGumpage(gumpageds);
	delete gumpageds;
#endif

	IDataSource *dummyds = filesystem->ReadFile("@data/empty.flx");
	_music = nullptr; //new MusicFlex(dummyds);
	delete dummyds;
#if 0
	IDataSource *mf = filesystem->ReadFile("@game/sound/_music.flx");
	if (!mf)
		error("Unable to load sound/_music.flx");

	_music = new MusicFlex(mf);
#endif

	dummyds = filesystem->ReadFile("@data/empty.flx");
	_soundFlex = new SoundFlex(dummyds);
	delete dummyds;
#if 0
	IDataSource *sndflx = filesystem->ReadFile("@game/sound/sound.flx");
	if (!sndflx)
		error("Unable to load sound/sound.flx");

	_soundFlex = new SoundFlex(sndflx);
#endif

	loadTranslation();
}

} // End of namespace Ultima8
} // End of namespace Ultima