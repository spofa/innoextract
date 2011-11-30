
#include "setup/info.hpp"

#include <cassert>
#include <istream>

#include <boost/foreach.hpp>

#include "setup/component.hpp"
#include "setup/data.hpp"
#include "setup/delete.hpp"
#include "setup/directory.hpp"
#include "setup/file.hpp"
#include "setup/icon.hpp"
#include "setup/ini.hpp"
#include "setup/item.hpp"
#include "setup/language.hpp"
#include "setup/message.hpp"
#include "setup/permission.hpp"
#include "setup/registry.hpp"
#include "setup/run.hpp"
#include "setup/task.hpp"
#include "setup/type.hpp"
#include "stream/block.hpp"
#include "util/load.hpp"
#include "util/log.hpp"

namespace setup {

namespace {

template <class Entry>
static void load_entries(std::istream & is, const setup::version & version,
                  info::entry_types entry_types, size_t count,
                  std::vector<Entry> & entries, info::entry_types::enum_type entry_type) {
	
	if(entry_types & entry_type) {
		entries.resize(count);
		for(size_t i = 0; i < count; i++) {
			Entry & entry = entries[i];
			entry.load(is, version);
		}
	} else {
		for(size_t i = 0; i < count; i++) {
			Entry entry;
			entry.load(is, version);
		}
	}
}

static void load_wizard_and_decompressor(std::istream & is, const setup::version & version,
                                        const setup::header & header,
                                        setup::info & info, info::entry_types entries) {
	
	(void)entries;
	
	is >> binary_string(info.wizard_image);
	
	if(version >= INNO_VERSION(2, 0, 0)) {
		is >> binary_string(info.wizard_image_small);
	}
	
	if(header.compression == stream::BZip2
	   || (header.compression == stream::LZMA1 && version == INNO_VERSION(4, 1, 5))
	   || (header.compression == stream::Zlib && version >= INNO_VERSION(4, 2, 6))) {
		
		is >> binary_string(info.decompressor_dll);
	}
}

} // anonymous namespace

void info::load(std::istream & ifs, entry_types e, const setup::version & v) {
	
	stream::block_reader::pointer is = stream::block_reader::get(ifs, v);
	assert(is);
	is->exceptions(std::ios_base::badbit | std::ios_base::failbit);
	
	header.load(*is, v);
	
	load_entries(*is, v, e, header.language_count, languages, Languages);
	
	if(v < INNO_VERSION(4, 0, 0)) {
		load_wizard_and_decompressor(*is, v, header, *this, e);
	}
	
	load_entries(*is, v, e, header.message_count, messages, Messages);
	load_entries(*is, v, e, header.permission_count, permissions, Permissions);
	load_entries(*is, v, e, header.type_count, types, Types);
	load_entries(*is, v, e, header.component_count, components, Components);
	load_entries(*is, v, e, header.task_count, tasks, Tasks);
	load_entries(*is, v, e, header.directory_count, directories, Directories);
	load_entries(*is, v, e, header.file_count, files, Files);
	load_entries(*is, v, e, header.icon_count, icons, Icons);
	load_entries(*is, v, e, header.ini_entry_count, ini_entries, IniEntries);
	load_entries(*is, v, e, header.registry_entry_count, registry_entries, RegistryEntries);
	load_entries(*is, v, e, header.delete_entry_count, delete_entries, DeleteEntries);
	load_entries(*is, v, e, header.uninstall_delete_entry_count, uninstall_delete_entries,
	             UninstallDeleteEntries);
	load_entries(*is, v, e, header.run_entry_count, run_entries, RunEntries);
	load_entries(*is, v, e, header.uninstall_run_entry_count, uninstall_run_entries,
	             UninstallRunEntries);
	
	if(v >= INNO_VERSION(4, 0, 0)) {
		load_wizard_and_decompressor(*is, v, header, *this, e);
	}
	
	is->exceptions(std::ios_base::goodbit);
	char dummy;
	if(!is->get(dummy).eof()) {
		throw std::ios_base::failure("expected end of stream");
	}
	
	// TODO skip to end if not there yet
	
	is = stream::block_reader::get(ifs, v);
	assert(is);
	is->exceptions(std::ios_base::badbit | std::ios_base::failbit);
	
	load_entries(*is, v, e, header.data_entry_count, data_entries, DataEntries);
	
	is->exceptions(std::ios_base::goodbit);
	if(!is->get(dummy).eof()) {
		throw std::ios_base::failure("expected end of stream");
	}
	
	if(e & Messages) {
		BOOST_FOREACH(setup::message_entry & entry, messages) {
			
			if(entry.language >= 0 ? size_t(entry.language) >= languages.size()
														: entry.language != -1) {
				log_warning << "unexpected language index: " << entry.language;
				continue;
			}
			
			uint32_t codepage;
			if(entry.language < 0) {
				codepage = v.codepage();
			} else {
				codepage = languages[size_t(entry.language)].codepage;
			}
			
			std::string decoded;
			to_utf8(entry.value, decoded, codepage);
			entry.value = decoded;
		}
	}
}

void info::load(std::istream & is, entry_types entries) {
	
	version.load(is);
	
	bool ambiguous = version.is_ambiguous();
	if(ambiguous) {
		entries |= NoSkip;
	}
	
	if(!version.known || ambiguous) {
		std::ios_base::streampos start = is.tellg();
		try {
			load(is, entries, version);
			return;
		} catch(...) {
			version.value = version.next();
			if(!version) {
				throw;
			}
			is.seekg(start);
		}
	}
	
	load(is, entries, version);
}

info::info() { }
info::~info() { }

} // namespace setup