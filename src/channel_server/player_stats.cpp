/*
Copyright (C) 2008-2016 Vana Development Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "player_stats.hpp"
#include "common/algorithm.hpp"
#include "common/data/type/player_stats_update.hpp"
#include "common/data/provider/equip.hpp"
#include "common/data/provider/skill.hpp"
#include "common/inter_header.hpp"
#include "common/packet_reader.hpp"
#include "common/packet_wrapper.hpp"
#include "common/util/game_logic/job.hpp"
#include "common/util/misc.hpp"
#include "common/util/randomizer.hpp"
#include "channel_server/channel_server.hpp"
#include "channel_server/instance.hpp"
#include "channel_server/inventory.hpp"
#include "channel_server/inventory_packet.hpp"
#include "channel_server/levels_packet.hpp"
#include "channel_server/maps.hpp"
#include "channel_server/party.hpp"
#include "channel_server/player.hpp"
#include "channel_server/player_data_provider.hpp"
#include "channel_server/player_packet.hpp"
#include "channel_server/players_packet.hpp"
#include "channel_server/summon_handler.hpp"
#include <iostream>
#include <limits>
#include <string>

namespace vana {
namespace channel_server {

player_stats::player_stats(ref_ptr<player> player, game_player_level level, game_job_id job, game_fame fame, game_stat str, game_stat dex, game_stat intl, game_stat luk, game_stat ap, game_health_ap hp_mp_ap, game_stat sp, game_health hp, game_health max_hp, game_health mp, game_health max_mp, game_experience exp) :
	m_player{player},
	m_level{level},
	m_job{job},
	m_fame{fame},
	m_str{str},
	m_dex{dex},
	m_int{intl},
	m_luk{luk},
	m_ap{ap},
	m_hp_mp_ap{hp_mp_ap},
	m_sp{sp},
	m_hp{hp},
	m_max_hp{max_hp},
	m_mp{mp},
	m_max_mp{max_mp},
	m_exp{exp}
{
	if (is_dead()) {
		m_hp = constant::stat::default_hp;
	}
}

auto player_stats::is_dead() const -> bool {
	return m_hp == constant::stat::min_hp;
}

// Equip stat bonus handling
auto player_stats::update_bonuses(bool update_equips, bool is_loading) -> void {
	if (m_maple_warrior > 0) {
		set_maple_warrior(m_maple_warrior);
	}

	if (auto player = m_player.lock()) {
		if (update_equips) {
			m_equip_bonuses = bonus_set();
			for (const auto &kvp : m_equip_stats) {
				const equip_bonus &info = kvp.second;
				if (channel_server::get_instance().get_equip_data_provider().can_equip(info.id, player->get_gender(), get_job(), get_str(true), get_dex(true), get_int(true), get_luk(true), get_fame())) {
					m_equip_bonuses.hp += info.hp;
					m_equip_bonuses.mp += info.mp;
					m_equip_bonuses.str += info.str;
					m_equip_bonuses.dex += info.dex;
					m_equip_bonuses.intl += info.intl;
					m_equip_bonuses.luk += info.luk;
				}
			}
		}
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");

	if (m_hyper_body_x > 0) {
		set_hyper_body_hp(m_hyper_body_x);
	}
	if (m_hyper_body_y > 0) {
		set_hyper_body_mp(m_hyper_body_y);
	}

	if (!is_loading) {
		// Adjust current HP/MP down if necessary
		if (get_hp() > get_max_hp()) {
			set_hp(get_hp());
		}
		if (get_mp() > get_max_mp()) {
			set_mp(get_mp());
		}
	}
}

auto player_stats::set_equip(game_inventory_slot slot, item *equip, bool is_loading) -> void {
	slot = std::abs(slot);
	if (equip != nullptr) {
		m_equip_stats[slot].id = equip->get_id();
		m_equip_stats[slot].hp = equip->get_hp();
		m_equip_stats[slot].mp = equip->get_mp();
		m_equip_stats[slot].str = equip->get_str();
		m_equip_stats[slot].dex = equip->get_dex();
		m_equip_stats[slot].intl = equip->get_int();
		m_equip_stats[slot].luk = equip->get_luk();
	}
	else {
		m_equip_stats.erase(slot);
	}

	update_bonuses(true, is_loading);
}

// Data acquisition
auto player_stats::connect_packet(packet_builder &builder) -> void {
	builder.add<game_player_level>(get_level());
	builder.add<game_job_id>(get_job());
	builder.add<game_stat>(get_str());
	builder.add<game_stat>(get_dex());
	builder.add<game_stat>(get_int());
	builder.add<game_stat>(get_luk());
	builder.add<game_health>(get_hp());
	builder.add<game_health>(get_max_hp(true));
	builder.add<game_health>(get_mp());
	builder.add<game_health>(get_max_mp(true));
	builder.add<game_stat>(get_ap());
	builder.add<game_stat>(get_sp());
	builder.add<game_experience>(get_exp());
	builder.add<game_fame>(get_fame());
}

auto player_stats::get_max_hp(bool without_bonus) -> game_health {
	if (!without_bonus) {
		return static_cast<game_health>(std::min<int32_t>(m_max_hp + m_equip_bonuses.hp + m_buff_bonuses.hp, constant::stat::max_max_hp));
	}
	return m_max_hp;
}

auto player_stats::get_max_mp(bool without_bonus) -> game_health {
	if (!without_bonus) {
		return static_cast<game_health>(std::min<int32_t>(m_max_mp + m_equip_bonuses.mp + m_buff_bonuses.mp, constant::stat::max_max_mp));
	}
	return m_max_mp;
}

auto player_stats::stat_utility(int32_t test) -> int16_t {
	return static_cast<int16_t>(std::min<int32_t>(std::numeric_limits<int16_t>::max(), test));
}

auto player_stats::get_str(bool with_bonus) -> game_stat {
	if (with_bonus) {
		return stat_utility(m_str + m_buff_bonuses.str + m_equip_bonuses.str);
	}
	return m_str;
}

auto player_stats::get_dex(bool with_bonus) -> game_stat {
	if (with_bonus) {
		return stat_utility(m_dex + m_buff_bonuses.dex + m_equip_bonuses.dex);
	}
	return m_dex;
}

auto player_stats::get_int(bool with_bonus) -> game_stat {
	if (with_bonus) {
		return stat_utility(m_int + m_buff_bonuses.intl + m_equip_bonuses.intl);
	}
	return m_int;
}

auto player_stats::get_luk(bool with_bonus) -> game_stat {
	if (with_bonus) {
		return stat_utility(m_luk + m_buff_bonuses.luk + m_equip_bonuses.luk);
	}
	return m_luk;
}

// Data modification
auto player_stats::check_hp_mp() -> void {
	if (m_hp > get_max_hp()) {
		m_hp = get_max_hp();
	}
	if (m_mp > get_max_mp()) {
		m_mp = get_max_mp();
	}
}

auto player_stats::set_level(game_player_level level) -> void {
	m_level = level;
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.level = level;
		player->send(packets::player::update_stat(updates, constant::stat::level));
		player->send_map(packets::level_up(player->get_id()));
		channel_server::get_instance().get_player_data_provider().update_player_level(player);
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_hp(game_health hp, bool send_packet) -> void {
	m_hp = ext::constrain_range<game_health>(hp, constant::stat::min_hp, get_max_hp());
	if (send_packet) {
		if (auto player = m_player.lock()) {
			data::type::player_stats_update updates;
			updates.hp = m_hp;
			player->send(packets::player::update_stat(updates, constant::stat::hp));
		}
		else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
	}
	modified_hp();
}

auto player_stats::modify_hp(int32_t hp_mod, bool send_packet) -> void {
	int32_t temp_hp = m_hp + hp_mod;
	temp_hp = ext::constrain_range<int32_t>(temp_hp, constant::stat::min_hp, get_max_hp());
	m_hp = static_cast<game_health>(temp_hp);

	if (send_packet) {
		if (auto player = m_player.lock()) {
			data::type::player_stats_update updates;
			updates.hp = m_hp;
			player->send(packets::player::update_stat(updates, constant::stat::hp));
		}
		else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
	}
	modified_hp();
}

auto player_stats::damage_hp(int32_t damage_hp) -> void {
	m_hp = std::max<int32_t>(constant::stat::min_hp, static_cast<int32_t>(m_hp) - damage_hp);
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.hp = m_hp;
		player->send(packets::player::update_stat(updates, constant::stat::hp));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
	modified_hp();
}

auto player_stats::modified_hp() -> void {
	if (auto player = m_player.lock()) {
		if (party *p = player->get_party()) {
			p->show_hp_bar(player);
		}
		player->get_active_buffs()->check_berserk();
		if (m_hp == constant::stat::min_hp) {
			if (instance *inst = player->get_instance()) {
				inst->player_death(player->get_id());
			}
			lose_exp();
			player->get_summons()->for_each([player](summon *summon) {
				summon_handler::remove_summon(player, summon->get_id(), false, summon_messages::disappearing);
			});
		}
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_mp(game_health mp, bool send_packet) -> void {
	if (auto player = m_player.lock()) {
		if (!player->get_active_buffs()->has_infinity()) {
			m_mp = ext::constrain_range<game_health>(mp, constant::stat::min_mp, get_max_mp());
		}
		data::type::player_stats_update updates;
		updates.mp = m_mp;
		player->send(packets::player::update_stat(updates, constant::stat::mp, send_packet));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::modify_mp(int32_t mp_mod, bool send_packet) -> void {
	if (auto player = m_player.lock()) {
		if (!player->get_active_buffs()->has_infinity()) {
			int32_t temp_mp = m_mp + mp_mod;
			temp_mp = ext::constrain_range<int32_t>(temp_mp, constant::stat::min_mp, get_max_mp());
			m_mp = static_cast<game_health>(temp_mp);
		}
		data::type::player_stats_update updates;
		updates.mp = m_mp;
		player->send(packets::player::update_stat(updates, constant::stat::mp, send_packet));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::damage_mp(int32_t damage_mp) -> void {
	if (auto player = m_player.lock()) {
		if (!player->get_active_buffs()->has_infinity()) {
			m_mp = std::max<int32_t>(constant::stat::min_mp, static_cast<int32_t>(m_mp) - damage_mp);
		}
		data::type::player_stats_update updates;
		updates.mp = m_mp;
		player->send(packets::player::update_stat(updates, constant::stat::mp));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_sp(game_stat sp) -> void {
	m_sp = sp;
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.sp = m_sp;
		player->send(packets::player::update_stat(updates, constant::stat::sp));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_ap(game_stat ap) -> void {
	m_ap = ap;
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.ap = m_ap;
		player->send(packets::player::update_stat(updates, constant::stat::ap));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_job(game_job_id job) -> void {
	m_job = job;
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.job = m_job;
		player->send(packets::player::update_stat(updates, constant::stat::job));
		player->send_map(packets::job_change(player->get_id()));
		channel_server::get_instance().get_player_data_provider().update_player_job(player);
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_str(game_stat str) -> void {
	m_str = str;
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.str = m_str;
		player->send(packets::player::update_stat(updates, constant::stat::str));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_dex(game_stat dex) -> void {
	m_dex = dex;
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.dex = m_dex;
		player->send(packets::player::update_stat(updates, constant::stat::dex));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_int(game_stat intl) -> void {
	m_int = intl;
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.intl = m_int;
		player->send(packets::player::update_stat(updates, constant::stat::intl));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_luk(game_stat luk) -> void {
	m_luk = luk;
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.luk = m_luk;
		player->send(packets::player::update_stat(updates, constant::stat::luk));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_maple_warrior(int16_t mod) -> void {
	m_buff_bonuses.str = (m_str * mod) / 100;
	m_buff_bonuses.dex = (m_dex * mod) / 100;
	m_buff_bonuses.intl = (m_int * mod) / 100;
	m_buff_bonuses.luk = (m_luk * mod) / 100;
	if (m_maple_warrior != mod) {
		m_maple_warrior = mod;
		update_bonuses();
	}
}

auto player_stats::set_max_hp(game_health max_hp) -> void {
	m_max_hp = ext::constrain_range(max_hp, constant::stat::min_max_hp, constant::stat::max_max_hp);
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.max_hp = m_max_hp;
		player->send(packets::player::update_stat(updates, constant::stat::max_hp));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
	modified_hp();
}

auto player_stats::set_max_mp(game_health max_mp) -> void {
	m_max_mp = ext::constrain_range(max_mp, constant::stat::min_max_mp, constant::stat::max_max_mp);
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.max_mp = m_max_mp;
		player->send(packets::player::update_stat(updates, constant::stat::max_mp));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_hyper_body_hp(int16_t mod) -> void {
	m_hyper_body_x = mod;
	m_buff_bonuses.hp = std::min<uint16_t>((m_max_hp + m_equip_bonuses.hp) * mod / 100, constant::stat::max_max_hp);
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.max_hp = m_max_hp;
		player->send(packets::player::update_stat(updates, constant::stat::max_hp));
		if (mod == 0) {
			set_hp(get_hp());
		}
		if (party *p = player->get_party()) {
			p->show_hp_bar(player);
		}
		player->get_active_buffs()->check_berserk();
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_hyper_body_mp(int16_t mod) -> void {
	m_hyper_body_y = mod;
	m_buff_bonuses.mp = std::min<uint16_t>((m_max_mp + m_equip_bonuses.mp) * mod / 100, constant::stat::max_max_mp);
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.max_mp = m_max_mp;
		player->send(packets::player::update_stat(updates, constant::stat::max_mp));
		if (mod == 0) {
			set_mp(get_mp());
		}
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::modify_max_hp(game_health mod) -> void {
	m_max_hp = std::min<game_health>(m_max_hp + mod, constant::stat::max_max_hp);
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.max_hp = m_max_hp;
		player->send(packets::player::update_stat(updates, constant::stat::max_hp));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::modify_max_mp(game_health mod) -> void {
	m_max_mp = std::min<game_health>(m_max_mp + mod, constant::stat::max_max_mp);
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.max_mp = m_max_mp;
		player->send(packets::player::update_stat(updates, constant::stat::max_mp));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_exp(game_experience exp) -> void {
	m_exp = std::max(exp, 0);
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.exp = m_exp;
		player->send(packets::player::update_stat(updates, constant::stat::exp));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::set_fame(game_fame fame) -> void {
	m_fame = ext::constrain_range(fame, constant::stat::min_fame, constant::stat::max_fame);
	if (auto player = m_player.lock()) {
		data::type::player_stats_update updates;
		updates.fame = m_fame;
		player->send(packets::player::update_stat(updates, constant::stat::fame));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::lose_exp() -> void {
	if (auto player = m_player.lock()) {
		if (!vana::util::game_logic::job::is_beginner_job(get_job()) && 
			get_level() < vana::util::game_logic::job::get_max_level(get_job()) && 
			// TODO: Really? Sorcerers room has no EXP loss?
			player->get_map_id() != constant::map::sorcerers_room) {
			game_slot_qty charms = player->get_inventory()->get_item_amount(constant::item::safety_charm);
			if (charms > 0) {
				inventory::take_item(player, constant::item::safety_charm, 1);
				// TODO FIXME REVIEW
				charms = --charms;
				player->send(packets::inventory::use_charm(static_cast<uint8_t>(charms)));
				return;
			}
			map *loc = player->get_map();
			double_t exp_loss;
			if (loc->lose_one_percent()) {
				exp_loss = 0.01;
			}
			else {
				if (vana::util::game_logic::job::get_job_line(get_job()) == constant::job::line::magician) {
					exp_loss = 0.08;
				}
				else {
					exp_loss = 0.2;
				}

				exp_loss = exp_loss / static_cast<double_t>(player->get_stats()->get_luk(true)) + 0.05;
			}
			game_experience exp = get_exp();
			exp -= static_cast<game_experience>(static_cast<double_t>(get_exp(get_level())) * exp_loss);
			set_exp(exp);
		}
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

// Level related functions
auto player_stats::give_exp(uint64_t exp, bool in_chat, bool white) -> void {
	if (auto player = m_player.lock()) {
		game_job_id full_job = get_job();
		game_player_level level = get_level();
		game_player_level job_max = vana::util::game_logic::job::get_max_level(full_job);
		if (level >= job_max) {
			// Do not give EXP to characters of max level or over
			return;
		}

		uint64_t cur_exp = get_exp() + exp;
		if (exp > 0) {
			uint64_t exp_counter = exp;
			uint64_t batch_size = std::numeric_limits<game_experience>::max();
			while (exp_counter > 0) {
				game_experience allocate = static_cast<game_experience>(std::min(exp_counter, batch_size));
				player->send(packets::show_exp(allocate, white, in_chat));
				exp_counter -= allocate;
			}
		}

		if (cur_exp >= get_exp(level)) {
			bool cygnus = vana::util::game_logic::job::is_cygnus(full_job);
			game_player_level levels_gained = 0;
			game_player_level levels_max = channel_server::get_instance().get_config().max_multi_level;
			game_stat ap_gain = 0;
			game_stat sp_gain = 0;
			game_health hp_gain = 0;
			game_health mp_gain = 0;
			int8_t job = vana::util::game_logic::job::get_job_track(get_job() % 1000);

			while (cur_exp >= get_exp(level) && levels_gained < levels_max) {
				cur_exp -= get_exp(get_level());
				level++;
				levels_gained++;

				const constant::stat::hp_mp_formula_arguments formula = constant::stat::hp_mp_formula[job][0];

				hp_gain += vana::util::randomizer::rand<game_health>(formula.hp_max, formula.hp_min);
				mp_gain += vana::util::randomizer::rand<game_health>(formula.mp_max, formula.mp_min);

				// Additional buffing through INT stats
				mp_gain += player->get_stats()->get_int(true) * formula.mp_int_stat_multiplier / 200;

				if (player->get_skills()->has_hp_increase()) {
					hp_gain += get_y(player->get_skills()->get_hp_increase());
				}
				if (player->get_skills()->has_mp_increase()) {
					mp_gain += get_y(player->get_skills()->get_mp_increase());
				}


				if (cygnus && level <= constant::stat::cygnus_ap_cutoff) {
					ap_gain += constant::stat::ap_per_cygnus_level;
				}
				else {
					ap_gain += constant::stat::ap_per_level;
				}

				if (!vana::util::game_logic::job::is_beginner_job(full_job)) {
					sp_gain += constant::stat::sp_per_level;
				}

				if (level >= job_max) {
					// Do not let people level past the level cap
					cur_exp = 0;
					break;
				}
			}

			if (cur_exp >= get_exp(level)) {
				// If the desired number of level ups have passed and they're still above, we chop
				cur_exp = get_exp(level) - 1;
			}

			// Check if the player has leveled up at all, it is possible that the user hasn't leveled up if multi-level limit is 0
			if (levels_gained) {
				modify_max_hp(hp_gain);
				modify_max_mp(mp_gain);
				set_level(level);
				set_ap(get_ap() + ap_gain);
				set_sp(get_sp() + sp_gain);

				// Let Hyper Body remain on during a level up, as it should
				auto active_buffs = player->get_active_buffs();
				auto hyper_body_hp = active_buffs->get_hyper_body_hp_source();
				auto hyper_body_mp = active_buffs->get_hyper_body_mp_source();
				if (hyper_body_hp.is_initialized()) {
					set_hyper_body_hp(active_buffs->get_buff_skill_info(hyper_body_hp.get())->x);
				}
				if (hyper_body_mp.is_initialized()) {
					set_hyper_body_mp(active_buffs->get_buff_skill_info(hyper_body_hp.get())->y);
				}

				set_hp(get_max_hp());
				set_mp(get_max_mp());
				player->set_level_date();
				if (get_level() == job_max && !player->is_gm()) {
					out_stream message;
					message << "[Congrats] " << player->get_name() << " has reached Level "
							<< static_cast<int16_t>(job_max) << "! Congratulate "
							<< player->get_name() << " on such an amazing achievement!";

					channel_server::get_instance().send_world(
						vana::packets::prepend(packets::player::show_message(message.str(), packets::player::notice_types::blue), [](packet_builder &builder) {
							builder.add<packet_header>(IMSG_TO_ALL_CHANNELS);
							builder.add<packet_header>(IMSG_TO_ALL_PLAYERS);
						}));
				}
			}
		}

		// By this point, the EXP should be a valid EXP in the range of 0 to game_experience max
		set_exp(static_cast<game_experience>(cur_exp));
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::add_stat(packet_reader &reader) -> void {
	reader.skip<game_tick_count>();
	int32_t type = reader.get<int32_t>();
	if (get_ap() == 0) {
		// Hacking
		return;
	}
	if (auto player = m_player.lock()) {
		player->send(packets::stat_ok());
		add_stat(type);
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::add_stat_multi(packet_reader &reader) -> void {
	reader.skip<game_tick_count>();
	uint32_t amount = reader.get<uint32_t>();

	if (auto player = m_player.lock()) {
		player->send(packets::stat_ok());

		for (uint32_t i = 0; i < amount; i++) {
			int32_t type = reader.get<int32_t>();
			int32_t value = reader.get<int32_t>();

			if (value < 0 || get_ap() < value) {
				// Hacking
				return;
			}

			add_stat(type, static_cast<int16_t>(value)); // Prefer a single cast to countless casts/modification down the line
		}
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::add_stat(int32_t type, int16_t mod, bool is_reset) -> void {
	game_stat max_stat = channel_server::get_instance().get_config().max_stats;
	bool is_subtract = mod < 0;
	if (auto player = m_player.lock()) {
		switch (type) {
			case constant::stat::str:
				if (get_str() >= max_stat) {
					return;
				}
				set_str(get_str() + mod);
				break;
			case constant::stat::dex:
				if (get_dex() >= max_stat) {
					return;
				}
				set_dex(get_dex() + mod);
				break;
			case constant::stat::intl:
				if (get_int() >= max_stat) {
					return;
				}
				set_int(get_int() + mod);
				break;
			case constant::stat::luk:
				if (get_luk() >= max_stat) {
					return;
				}
				set_luk(get_luk() + mod);
				break;
			case constant::stat::max_hp:
			case constant::stat::max_mp: {
				if (type == constant::stat::max_hp && get_max_hp(true) >= constant::stat::max_max_hp) {
					return;
				}
				if (type == constant::stat::max_mp && get_max_mp(true) >= constant::stat::max_max_mp) {
					return;
				}
				if (is_subtract && get_hp_mp_ap() == 0) {
					// Hacking
					return;
				}
				int8_t job = vana::util::game_logic::job::get_job_track(get_job() % 1000);

				const constant::stat::hp_mp_formula_arguments formula = constant::stat::hp_mp_formula[job][1];

				game_health hp_gain = vana::util::randomizer::rand<game_health>(formula.hp_max, formula.hp_min);
				game_health mp_gain = vana::util::randomizer::rand<game_health>(formula.mp_max, formula.mp_min);

				// Additional buffing through INT stats
				mp_gain += player->get_stats()->get_int(true) * formula.mp_int_stat_multiplier / 200;

				if (player->get_skills()->has_hp_increase()) {
					hp_gain += get_y(player->get_skills()->get_hp_increase());
				}
				if (player->get_skills()->has_mp_increase()) {
					mp_gain += get_y(player->get_skills()->get_mp_increase());
				}

				set_hp_mp_ap(get_hp_mp_ap() + mod);
				switch (type) {
					case constant::stat::max_hp: modify_max_hp(hp_gain); break;
					case constant::stat::max_mp: modify_max_mp(mp_gain); break;
				}

				auto active_buffs = player->get_active_buffs();
				auto hyper_body_hp = active_buffs->get_hyper_body_hp_source();
				auto hyper_body_mp = active_buffs->get_hyper_body_mp_source();
				if (hyper_body_hp.is_initialized()) {
					set_hyper_body_hp(active_buffs->get_buff_skill_info(hyper_body_hp.get())->x);
				}
				if (hyper_body_mp.is_initialized()) {
					set_hyper_body_mp(active_buffs->get_buff_skill_info(hyper_body_hp.get())->y);
				}

				set_hp(get_hp());
				set_mp(get_mp());
				break;
			}
			default:
				// Hacking, one assumes
				break;
		}
		if (!is_reset) {
			set_ap(get_ap() - mod);
		}
		update_bonuses();
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::get_x(game_skill_id skill_id) -> int16_t {
	if (auto player = m_player.lock()) {
		return player->get_skills()->get_skill_info(skill_id)->x;
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::get_y(game_skill_id skill_id) -> int16_t {
	if (auto player = m_player.lock()) {
		return player->get_skills()->get_skill_info(skill_id)->y;
	}
	else THROW_CODE_EXCEPTION(invalid_operation_exception, "This should never be thrown");
}

auto player_stats::get_exp(game_player_level level) -> game_experience {
	return constant::stat::player_exp[level - 1];
}

}
}