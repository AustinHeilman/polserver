/** @file
 *
 * @par History
 * - 2005/03/05 Shinigami: format_description -> ServSpecOpt UseAAnTileFlags to enable/disable
 * "a"/"an" via Tiles.cfg in formatted Item Names
 * - 2005/04/03 Shinigami: send_feature_enable - added UOExpansionFlag for Samurai Empire
 * - 2005/08/29 Shinigami: ServSpecOpt UseAAnTileFlags renamed to UseTileFlagPrefix
 * - 2005/09/17 Shinigami: send_nametext - smaller bugfix in passert-check
 * - 2006/05/07 Shinigami: SendAOSTooltip - will now send merchant_description() if needed
 * - 2006/05/16 Shinigami: send_feature_enable - added UOExpansionFlag for Mondain's Legacy
 * - 2007/04/08 MuadDib:   Updated send_worn_item_to_inrange to create the message only
 *                         once and use the Transmit_to_inrange instead. Then uses
 *                         send_object_cache_to_inrange. Required a tooltips.* update.
 * - 2008/07/08 Turley:    get_flag1() changed to show WarMode of other player again
 *                         get_flag1_aos() removed
 * - 2009/07/23 MuadDib:   updates for new Enum::Packet Out ID
 * - 2009/07/31 Turley:    added send_fight_occuring() for packet 0x2F
 * - 2009/08/01 MuadDib:   Removed send_tech_stuff(), unused and obsolete.
 * - 2009/08/09 MuadDib:   UpdateCharacterWeight() Rewritten to send stat msg intead of refreshar().
 *                         Refactor of Packet 0x25 for naming convention
 * - 2009/08/14 Turley:    PKTOUT_B9_V2 removed unk u16 and changed flag to u32
 * - 2009/09/03 MuadDib:   Relocation of account related cpp/h
 *                         Relocation of multi related cpp/h
 * - 2009/09/13 MuadDib:   Optimized send_remove_character_to_nearby_cansee,
 * send_remove_character_to_nearby_cantsee, send_remove_character_to_nearby
 *                         to build packet and handle iter internally. Packet built just once this
 * way, and sent to those who need it.
 * - 2009/09/06 Turley:    Changed Version checks to bitfield client->ClientType
 * - 2009/09/22 MuadDib:   Adding resending of light level if override not in effect, to sending of
 * season packet. Fixes EA client bug.
 * - 2009/09/22 Turley:    Added DamagePacket support
 * - 2009/10/07 Turley:    Fixed DestroyItem while in hand
 * - 2009/10/12 Turley:    whisper/yell/say-range ssopt definition
 * - 2009/12/02 Turley:    0xf3 packet support - Tomi
 *                         face support
 * - 2009/12/03 Turley:    added 0x17 packet everywhere only send if poisoned, fixed get_flag1
 * (problem with poisoned & flying)
 */


#include "ufunc.h"

#include <cstddef>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "../bscript/impstr.h"
#include "../clib/clib_endian.h"
#include "../clib/logfacility.h"
#include "../clib/passert.h"
#include "../clib/stlutil.h"
#include "../plib/mapcell.h"
#include "../plib/objtype.h"
#include "../plib/systemstate.h"
#include "../plib/uoexpansion.h"
#include "accounts/account.h"
#include "containr.h"
#include "fnsearch.h"
#include "globals/network.h"
#include "globals/object_storage.h"
#include "globals/state.h"
#include "globals/uvars.h"
#include "item/item.h"
#include "layers.h"
#include "lightlvl.h"
#include "mobile/charactr.h"
#include "mobile/corpse.h"
#include "mobile/npc.h"
#include "multi/multi.h"
#include "multi/multidef.h"
#include "network/cgdata.h"
#include "network/client.h"
#include "network/packetdefs.h"
#include "network/packethelper.h"
#include "network/pktdef.h"
#include "objecthash.h"
#include "polclass.h"
#include "realms/realm.h"
#include "regions/miscrgn.h"
#include "statmsg.h"
#include "tooltips.h"
#include "uobject.h"
#include "uoclient.h"
#include "uworld.h"

namespace Pol
{
namespace Core
{
using namespace Network;
using namespace Mobile;
using namespace Items;

// Dave added 3/9/3
void SetCurrentItemSerialNumber( u32 serial )
{
  stateManager.itemserialnumber = serial;
}

// Dave added 3/9/3
void SetCurrentCharSerialNumber( u32 serial )
{
  stateManager.charserialnumber = serial;
}

// Dave added 3/8/3
u32 GetCurrentItemSerialNumber( void )
{
  return stateManager.itemserialnumber;
}

// Dave added 3/8/3
u32 GetCurrentCharSerialNumber( void )
{
  return stateManager.charserialnumber;
}

// Dave changed 3/8/3 to use objecthash
u32 GetNextSerialNumber( void )
{
  u32 nextserial = objStorageManager.objecthash.GetNextUnusedCharSerial();
  stateManager.charserialnumber = nextserial;
  return stateManager.charserialnumber;
}

u32 UseCharSerialNumber( u32 serial )
{
  if ( serial > stateManager.charserialnumber )
    stateManager.charserialnumber = serial + 1;
  return serial;
}

// Dave changed 3/8/3
u32 UseItemSerialNumber( u32 serial )
{
  if ( serial > stateManager.itemserialnumber )
    stateManager.itemserialnumber = serial + 1;
  return serial;
}

// Dave changed 3/8/3 to use objecthash
u32 GetNewItemSerialNumber( void )
{
  u32 nextserial = objStorageManager.objecthash.GetNextUnusedItemSerial();
  stateManager.itemserialnumber = nextserial;
  return stateManager.itemserialnumber;
}

void send_goxyz( Client* client, const Character* chr )
{
  PktHelper::PacketOut<PktOut_20> msg;
  msg->Write<u32>( chr->serial_ext );
  msg->WriteFlipped<u16>( chr->graphic );
  msg->offset++;  // unk7
  msg->WriteFlipped<u16>( chr->color );
  msg->Write<u8>( chr->get_flag1( client ) );
  msg->WriteFlipped<u16>( chr->x() );
  msg->WriteFlipped<u16>( chr->y() );
  msg->offset += 2;                       // unk15,16
  msg->Write<u8>( 0x80u | chr->facing );  // is it always right to set this flag?
  msg->Write<s8>( chr->z() );
  msg.Send( client );

  if ( ( client->ClientType & CLIENTTYPE_UOKR ) &&
       ( chr->poisoned() ) )  // if poisoned send 0x17 for newer clients
    send_poisonhealthbar( client, chr );

  if ( ( client->ClientType & CLIENTTYPE_UOKR ) &&
       ( chr->invul() ) )  // if invul send 0x17 for newer clients
    send_invulhealthbar( client, chr );
}

// Character chr has moved.  Tell a client about it.
void send_move( Client* client, const Character* chr )
{
  MoveChrPkt msgmove( chr );
  msgmove.Send( client );

  if ( chr->poisoned() )  // if poisoned send 0x17 for newer clients
    send_poisonhealthbar( client, chr );

  if ( chr->invul() )  // if invul send 0x17 for newer clients
    send_invulhealthbar( client, chr );
}

void send_poisonhealthbar( Client* client, const Character* chr )
{
  if ( client->ClientType & Network::CLIENTTYPE_UOKR )
  {
    Network::HealthBarStatusUpdate msg(
        chr->serial_ext, Network::HealthBarStatusUpdate::Color::GREEN, chr->poisoned() );
    msg.Send( client );
  }
}

void send_invulhealthbar( Client* client, const Character* chr )
{
  if ( client->ClientType & Network::CLIENTTYPE_UOKR )
  {
    Network::HealthBarStatusUpdate msg(
        chr->serial_ext, Network::HealthBarStatusUpdate::Color::YELLOW, chr->invul() );
    msg.Send( client );
  }
}

void send_owncreate( Client* client, const Character* chr )
{
  PktHelper::PacketOut<PktOut_78> owncreate;
  owncreate->offset += 2;
  owncreate->Write<u32>( chr->serial_ext );
  owncreate->WriteFlipped<u16>( chr->graphic );
  owncreate->WriteFlipped<u16>( chr->x() );
  owncreate->WriteFlipped<u16>( chr->y() );
  owncreate->Write<s8>( chr->z() );
  owncreate->Write<u8>( chr->facing );
  owncreate->WriteFlipped<u16>( chr->color );
  owncreate->Write<u8>( chr->get_flag1( client ) );
  owncreate->Write<u8>( chr->hilite_color_idx( client->chr ) );

  for ( int layer = LAYER_EQUIP__LOWEST; layer <= LAYER_EQUIP__HIGHEST; ++layer )
  {
    const Item* item = chr->wornitem( layer );
    if ( item == nullptr )
      continue;

    // Dont send faces if older client or ssopt
    if ( ( layer == LAYER_FACE ) &&
         ( ( settingsManager.ssopt.features.faceSupport() == Plib::FaceSupport::None ) ||
           ( ~client->ClientType & CLIENTTYPE_UOKR ) ) )
      continue;

    if ( client->ClientType & CLIENTTYPE_70331 )
    {
      owncreate->Write<u32>( item->serial_ext );
      owncreate->WriteFlipped<u16>( item->graphic );
      owncreate->Write<u8>( static_cast<u8>( layer ) );
      owncreate->WriteFlipped<u16>( item->color );
    }
    else if ( item->color )
    {
      owncreate->Write<u32>( item->serial_ext );
      owncreate->WriteFlipped<u16>( 0x8000u | item->graphic );
      owncreate->Write<u8>( static_cast<u8>( layer ) );
      owncreate->WriteFlipped<u16>( item->color );
    }
    else
    {
      owncreate->Write<u32>( item->serial_ext );
      owncreate->WriteFlipped<u16>( item->graphic );
      owncreate->Write<u8>( static_cast<u8>( layer ) );
    }
  }
  owncreate->offset += 4;  // items nullterm
  u16 len = owncreate->offset;
  owncreate->offset = 1;
  owncreate->WriteFlipped<u16>( len );

  owncreate.Send( client, len );

  if ( client->acctSupports( Plib::ExpansionVersion::AOS ) )
  {
    send_object_cache( client, chr );
    // 07/11/09 Turley: moved to bottom first the client needs to know the item then we can send
    // revision
    for ( int layer = LAYER_EQUIP__LOWEST; layer <= LAYER_EQUIP__HIGHEST; ++layer )
    {
      const Item* item = chr->wornitem( layer );
      if ( item == nullptr )
        continue;
      if ( layer == LAYER_FACE )
        continue;
      send_object_cache( client, item );
    }
  }

  if ( chr->poisoned() )  // if poisoned send 0x17 for newer clients
    send_poisonhealthbar( client, chr );

  if ( chr->invul() )  // if invul send 0x17 for newer clients
    send_invulhealthbar( client, chr );
}

void build_owncreate( const Character* chr, PktOut_78* owncreate )
{
  owncreate->offset += 2;
  owncreate->Write<u32>( chr->serial_ext );
  owncreate->WriteFlipped<u16>( chr->graphic );
  owncreate->WriteFlipped<u16>( chr->x() );
  owncreate->WriteFlipped<u16>( chr->y() );
  owncreate->Write<s8>( chr->z() );
  owncreate->Write<u8>( chr->facing );
  owncreate->WriteFlipped<u16>( chr->color );  // 17
}
void send_owncreate( Client* client, const Character* chr, PktOut_78* owncreate )
{
  owncreate->offset = 17;
  owncreate->Write<u8>( chr->get_flag1( client ) );
  owncreate->Write<u8>( chr->hilite_color_idx( client->chr ) );

  for ( int layer = LAYER_EQUIP__LOWEST; layer <= LAYER_EQUIP__HIGHEST; ++layer )
  {
    const Item* item = chr->wornitem( layer );
    if ( item == nullptr )
      continue;

    // Dont send faces if older client or ssopt
    if ( ( layer == LAYER_FACE ) &&
         ( ( settingsManager.ssopt.features.faceSupport() == Plib::FaceSupport::None ) ||
           ( ~client->ClientType & CLIENTTYPE_UOKR ) ) )
      continue;

    if ( client->ClientType & CLIENTTYPE_70331 )
    {
      owncreate->Write<u32>( item->serial_ext );
      owncreate->WriteFlipped<u16>( item->graphic );
      owncreate->Write<u8>( static_cast<u16>( layer ) );
      owncreate->WriteFlipped<u16>( item->color );
    }
    else if ( item->color )
    {
      owncreate->Write<u32>( item->serial_ext );
      owncreate->WriteFlipped<u16>( 0x8000u | item->graphic );
      owncreate->Write<u8>( static_cast<u8>( layer ) );
      owncreate->WriteFlipped<u16>( item->color );
    }
    else
    {
      owncreate->Write<u32>( item->serial_ext );
      owncreate->WriteFlipped<u16>( item->graphic );
      owncreate->Write<u8>( static_cast<u8>( layer ) );
    }
  }
  owncreate->offset += 4;  // items nullterm
  u16 len = owncreate->offset;
  owncreate->offset = 1;
  owncreate->WriteFlipped<u16>( len );

  Core::networkManager.clientTransmit->AddToQueue( client, &owncreate->buffer, len );

  if ( client->acctSupports( Plib::ExpansionVersion::AOS ) )
  {
    send_object_cache( client, chr );
    // 07/11/09 Turley: moved to bottom first the client needs to know the item then we can send
    // revision
    for ( int layer = LAYER_EQUIP__LOWEST; layer <= LAYER_EQUIP__HIGHEST; ++layer )
    {
      const Item* item = chr->wornitem( layer );
      if ( item == nullptr )
        continue;
      if ( layer == LAYER_FACE )
        continue;
      send_object_cache( client, item );
    }
  }
}

void send_remove_character( Client* client, const Character* chr )
{
  if ( !client->ready ) /* if a client is just connecting, don't bother him. */
    return;

  /* Don't remove myself */
  if ( client->chr == chr )
    return;
  Network::RemoveObjectPkt msgremove( chr->serial_ext );
  msgremove.Send( client );
}

void send_remove_character( Network::Client* client, const Mobile::Character* chr,
                            Network::RemoveObjectPkt& pkt )
{
  if ( !client->ready ) /* if a client is just connecting, don't bother him. */
    return;

  /* Don't remove myself */
  if ( client->chr == chr )
    return;
  pkt.update( chr->serial_ext );
  pkt.Send( client );
}

void send_remove_character_to_nearby( const Character* chr )
{
  Network::RemoveObjectPkt msgremove( chr->serial_ext );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( chr,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr == chr )
                                                           return;
                                                         if ( zonechr->in_visual_range( chr ) )
                                                           msgremove.Send( zonechr->client );
                                                       } );
}

void send_remove_character_to_nearby_cantsee( const Character* chr )
{
  Network::RemoveObjectPkt msgremove( chr->serial_ext );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      chr,
      [&]( Character* zonechr )
      {
        if ( zonechr == chr )
          return;
        if ( !zonechr->in_visual_range( chr ) )
          return;
        if ( !zonechr->is_visible_to_me( chr, /*check_range*/ false ) )
          msgremove.Send( zonechr->client );
      } );
}

void send_remove_character_to_nearby_cansee( const Character* chr )
{
  Network::RemoveObjectPkt msgremove( chr->serial_ext );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      chr,
      [&]( Character* _chr )
      {
        if ( _chr != chr && _chr->is_visible_to_me( chr ) )
          msgremove.Send( _chr->client );
      } );
}

void send_remove_object_if_inrange( Client* client, const Item* item )
{
  if ( !client->ready ) /* if a client is just connecting, don't bother him. */
    return;

  if ( !client->chr->in_visual_range( item ) )
    return;
  Network::RemoveObjectPkt msgremove( item->serial_ext );
  msgremove.Send( client );
}

void send_remove_object( Client* client, const UObject* object )
{
  if ( client == nullptr || !client->ready )
    return;
  Network::RemoveObjectPkt msgremove( object->serial_ext );
  msgremove.Send( client );
}

void send_remove_object_to_inrange( const UObject* centerObject )
{
  Network::RemoveObjectPkt msgremove( centerObject->serial_ext );
  Core::WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      centerObject,
      [&]( Character* chr )
      {
        if ( chr->in_visual_range( centerObject ) )
          msgremove.Send( chr->client );
      } );
}

void send_remove_object( Client* client, const UObject* item, RemoveObjectPkt& pkt )
{
  if ( !client->ready ) /* if a client is just connecting, don't bother him. */
    return;
  pkt.update( item->serial_ext );
  pkt.Send( client );
}

void send_put_in_container( Client* client, const Item* item )
{
  auto msg = Network::AddItemContainerMsg(
      item->serial_ext, item->graphic, item->get_senditem_amount(), item->pos2d(),
      item->slot_index(), item->container->serial_ext, item->color );
  msg.Send( client );

  if ( client->acctSupports( Plib::ExpansionVersion::AOS ) )
    send_object_cache( client, item );
}

void send_put_in_container_to_inrange( const Item* item )
{
  auto msg = Network::AddItemContainerMsg(
      item->serial_ext, item->graphic, item->get_senditem_amount(), item->pos2d(),
      item->slot_index(), item->container->serial_ext, item->color );

  auto pkt_rev = Network::ObjRevisionPkt( item->serial_ext, item->rev() );

  // FIXME mightsee also checks remote containers thus the ForEachPlayer functions cannot be used
  for ( auto& client2 : networkManager.clients )
  {
    if ( !client2->ready )
      continue;
    // FIXME need to check character's additional_legal_items.
    // looks like inrange should be a Character member function.
    if ( client2->chr->mightsee( item->container ) )
    {
      // FIXME if the container has an owner, and I'm not it, don't tell me?
      msg.Send( client2 );
      pkt_rev.Send( client2 );
    }
  }
}

// An item is visible on a corpse if:
//   - it's visible
//   - or the chr has seeinvisitems() privilege
// (note: hair items are not invisible on corpses)
bool can_see_on_corpse( const Client* client, const Core::ItemRef& item )
{
  bool invisible = ( item->invisible() && !client->chr->can_seeinvisitems() );

  return !invisible;
}

// Helper function for send_corpse_items(). Sends packet 0x89 containing information
// of equipped items on the corpse.
void send_corpse_equip( Client* client, const UCorpse* corpse )
{
  PktHelper::PacketOut<PktOut_89> msg;
  msg->offset += 2;
  msg->Write<u32>( corpse->serial_ext );

  for ( unsigned layer = Core::LOWEST_LAYER; layer <= Core::HIGHEST_LAYER; ++layer )
  {
    const auto& item2 = corpse->GetItemOnLayer( layer );

    if ( !item2 || item2->orphan() || item2->container != corpse )
      continue;

    if ( !can_see_on_corpse( client, item2 ) )
      continue;

    msg->Write<u8>( item2->layer );
    msg->Write<u32>( item2->serial_ext );
  }

  msg->offset += 1;  // nullterm byte
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );

  msg.Send( client, len );
}

// Helper function for send_corpse_items(). No need to send the full corpse contents,
// just the equipped items. Uses packet 0x3C.
void send_corpse_contents( Client* client, const UCorpse* corpse )
{
  PktHelper::PacketOut<PktOut_3C> msg;
  msg->offset += 4;  // msglen+count
  u16 count = 0;

  for ( unsigned layer = Core::LOWEST_LAYER; layer <= Core::HIGHEST_LAYER; ++layer )
  {
    const Core::ItemRef item = corpse->GetItemOnLayer( layer );

    if ( !item || item->orphan() || item->container != corpse )
      continue;

    if ( !can_see_on_corpse( client, item ) )
      continue;

    msg->Write<u32>( item->serial_ext );
    msg->WriteFlipped<u16>( item->graphic );
    msg->offset++;  // unk6
    msg->WriteFlipped<u16>( item->get_senditem_amount() );
    msg->WriteFlipped<u16>( item->x() );
    msg->WriteFlipped<u16>( item->y() );
    if ( client->ClientType & CLIENTTYPE_6017 )
      msg->Write<u8>( item->slot_index() );
    msg->Write<u32>( corpse->serial_ext );
    msg->WriteFlipped<u16>( item->color );  // color
    ++count;
  }

  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg->WriteFlipped<u16>( count );
  msg.Send( client, len );
}

// FIXME it would be better to compose this message once and
// send to multiple clients.
//
// The corpse requires a packet (0x89) to say which items are equipped and another (0x3C)
// to describe the equipped items (similar packet as in the send_container_contents(), but
// just the outside items).

void send_corpse( Client* client, const Item* item )
{
  const UCorpse* corpse = static_cast<const UCorpse*>( item );
  // Send the contents _first_, so the client is aware of the copied items (eg.
  // hair). Then send the corpse equipment, as client knows about the copied
  // items.
  send_corpse_contents( client, corpse );
  send_corpse_equip( client, corpse );
}

// This function sends every item in the corpse, not only the equipped items. It's mainly
// used to tell the player that he's now dead and his items are in the corpse.
void send_full_corpse( Client* client, const Item* item )
{
  const UCorpse* corpse = static_cast<const UCorpse*>( item );
  send_corpse_equip( client, corpse );
  send_container_contents( client, *corpse );
}

void send_corpse_equip_inrange( const Item* item )
{
  const UCorpse* corpse = static_cast<const UCorpse*>( item );

  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( corpse,
                                                       [&]( Character* chr )
                                                       {
                                                         if ( chr->in_visual_range( corpse ) )
                                                           send_corpse_equip( chr->client, corpse );
                                                       } );
}

// Item::sendto( Client* ) ??
void send_item( Client* client, const Item* item )
{
  if ( item->invisible() && !client->chr->can_seeinvisitems() )
  {
    send_remove_object( client, item );
    return;
  }

  u8 flags = 0;
  if ( client->chr->can_move( item ) )
    flags |= ITEM_FLAG_FORCE_MOVABLE;
  if ( item->invisible() )
    flags |= ITEM_FLAG_HIDDEN;

  auto pkt = SendWorldItem( item->serial, item->graphic, item->get_senditem_amount(), item->pos3d(),
                            item->facing, item->color, flags );
  pkt.Send( client );

  // if the item is a corpse, transmit items contained by it
  if ( item->objtype_ == UOBJ_CORPSE )
  {
    send_corpse( client, item );
  }

  if ( client->acctSupports( Plib::ExpansionVersion::AOS ) )
  {
    send_object_cache( client, item );
    return;
  }
}

/* Tell all clients new information about an item */
void send_item_to_inrange( const Item* item )
{
  auto pkt = SendWorldItem( item->serial, item->graphic, item->get_senditem_amount(), item->pos3d(),
                            item->facing, item->color, 0 );
  auto pkt_remove = RemoveObjectPkt( item->serial_ext );
  auto pkt_rev = ObjRevisionPkt( item->serial_ext, item->rev() );

  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      item,
      [&]( Character* zonechr )
      {
        if ( !zonechr->in_visual_range( item ) )
          return;
        if ( item->invisible() && !zonechr->can_seeinvisitems() )
        {
          pkt_remove.Send( zonechr->client );
          return;
        }

        u8 flags = 0;
        if ( zonechr->can_move( item ) )
          flags |= ITEM_FLAG_FORCE_MOVABLE;
        if ( item->invisible() )
          flags |= ITEM_FLAG_HIDDEN;
        pkt.updateFlags( flags );
        pkt.Send( zonechr->client );

        // if the item is a corpse, transmit items contained by it
        if ( item->objtype_ == UOBJ_CORPSE )
        {
          send_corpse( zonechr->client, item );
        }

        pkt_rev.Send( zonechr->client );
      } );
}


void update_item_to_inrange( const Item* item )
{
  if ( item->container != nullptr )
  {
    if ( IsCharacter( item->container->serial ) )
    {
      // this may not be the right thing in all cases.
      // specifically, handle_dye used to not ever do send_wornitem.
      // FIXME way, way inefficient, but nontrivial.
      Character* chr = find_character( item->container->serial );
      if ( chr )
      {
        update_wornitem_to_inrange( chr, item );
      }
      else
        POLLOG_ERRORLN( "Ack! update_item_to_inrange: character {:#x} doesn't exist!",
                        item->container->serial );
    }
    else
    {
      send_put_in_container_to_inrange( item );
    }
  }
  else
  {
    send_item_to_inrange( item );
  }
}

void send_light( Client* client, int lightlevel )
{
  if ( VALID_LIGHTLEVEL( lightlevel ) )
  {
    PktHelper::PacketOut<PktOut_4F> msg;
    msg->Write<u8>( static_cast<u8>( lightlevel ) );
    msg.Send( client );
  }
}

void send_weather( Client* client, u8 type, u8 severity, u8 aux )
{
  PktHelper::PacketOut<PktOut_65> msg;
  msg->Write<u8>( type );
  msg->Write<u8>( severity );
  msg->Write<u8>( aux );
  msg.Send( client );
}

/* send_char_data: called once for each client when a new character enters
   the world. */
void send_char_data( Client* client, Character* chr )
{
  if ( !client->ready )
    return;

  if ( !client->chr->is_visible_to_me( chr ) )
    return;
  send_owncreate( client, chr );
}

/* send_client_char_data: called once for each character when a client
   logs on.  If in range, tell the client about each character. */
void send_client_char_data( Character* chr, Client* client )
{
  // Don't tell a client about its own character.
  if ( client->chr == chr )
    return;

  if ( !client->chr->is_visible_to_me( chr ) )
    return;

  send_owncreate( client, chr );
}

void send_item_move_failure( Network::Client* client, u8 reason )
{
  PktHelper::PacketOut<PktOut_27> msg;
  msg->Write<u8>( reason );
  msg.Send( client );
}

void send_wornitem( Client* client, const Character* chr, const Item* item )
{
  PktHelper::PacketOut<PktOut_2E> msg;
  msg->Write<u32>( item->serial_ext );
  msg->WriteFlipped<u16>( item->graphic );
  msg->offset++;  // unk7
  msg->Write<u8>( item->layer );
  msg->Write<u32>( chr->serial_ext );
  msg->WriteFlipped<u16>( item->color );
  msg.Send( client );

  if ( client->acctSupports( Plib::ExpansionVersion::AOS ) )
  {
    send_object_cache( client, item );
  }
}

void send_wornitem_to_inrange( const Character* chr, const Item* item )
{
  PktHelper::PacketOut<PktOut_2E> msg;
  msg->Write<u32>( item->serial_ext );
  msg->WriteFlipped<u16>( item->graphic );
  msg->offset++;  // unk7
  msg->Write<u8>( item->layer );
  msg->Write<u32>( chr->serial_ext );
  msg->WriteFlipped<u16>( item->color );
  transmit_to_inrange( item, &msg->buffer, msg->offset );
  send_object_cache_to_inrange( item );
}

// This used when item already worn and graphic/color changed. Deletes the item
// at client and then sends the new information.
void update_wornitem_to_inrange( const Character* chr, const Item* item )
{
  if ( chr != nullptr )
  {
    send_remove_object_to_inrange( item );

    PktHelper::PacketOut<PktOut_2E> msg;
    msg->Write<u32>( item->serial_ext );
    msg->WriteFlipped<u16>( item->graphic );
    msg->offset++;  // unk7
    msg->Write<u8>( item->layer );
    msg->Write<u32>( chr->serial_ext );
    msg->WriteFlipped<u16>( item->color );
    transmit_to_inrange( item, &msg->buffer, msg->offset );

    send_object_cache_to_inrange( item );
  }
}

// does 'item' have a parent with serial 'serial'?
bool is_a_parent( const Item* item, u32 serial )
{
  while ( item->container != nullptr )
  {
    // UNTESTED
    item = item->container;
    if ( item->serial == serial )
      return true;
  }
  return false;
}


// search for a container that this character can legally act upon
// - remove items, insert items, etc.
UContainer* find_legal_container( const Character* chr, u32 serial )
{
  UContainer* cont;
  cont = chr->backpack();
  if ( cont )
  {
    if ( serial == cont->serial )
      return cont;
    // not the main backpack, look for subpacks.
    cont = cont->find_container( serial );
    if ( cont )
      return cont;
  }

  // 4/2007 - MuadDib
  // Wasn't in backpack, check wornitems
  cont = nullptr;
  Item* worn_item = chr->find_wornitem( serial );
  if ( worn_item != nullptr && worn_item->script_isa( POLCLASS_CONTAINER ) )
  {
    // Ignore these layers explicitly. Backpack especially since it was
    // already checked above.
    if ( worn_item->layer != LAYER_HAIR && worn_item->layer != LAYER_FACE &&
         worn_item->layer != LAYER_BEARD && worn_item->layer != LAYER_BACKPACK &&
         worn_item->layer != LAYER_MOUNT )
    {
      UContainer* worn_cont = static_cast<UContainer*>( worn_item );
      if ( worn_cont != nullptr )
        return worn_cont;
    }
  }


  // not in the backpack, or in a subpack.  check global items and subpacks.
  // FIXME doesn't check range?
  Range2d gridarea( zone_convert( chr->pos() - Vec2d( 8, 8 ) ),
                    zone_convert( chr->pos() + Vec2d( 8, 8 ) ), nullptr );
  for ( const auto& gpos : gridarea )
  {
    for ( auto& item : chr->realm()->getzone_grid( gpos ).items )
    {
      if ( item->isa( UOBJ_CLASS::CLASS_CONTAINER ) )
      {
        cont = (UContainer*)item;
        if ( serial == cont->serial )
          return cont;
        cont = cont->find_container( serial );
        if ( cont )
          return cont;
      }
    }
  }

  if ( settingsManager.ssopt.master_can_access_npcs_backpack )
  {
    auto item = system_find_item( serial );
    if ( item )
    {
      // If the item is owned by by an NPC ...
      if ( auto owner = item->GetCharacterOwner(); owner && owner->isa( UOBJ_CLASS::CLASS_NPC ) )
      {
        auto npc = static_cast<Mobile::NPC*>( owner );

        // ... and chr is npc's master ...
        if ( npc->master() == chr )
        {
          // ... and the npc has a backpack ...
          cont = npc->backpack();
          if ( cont )
          {
            // ... it is legal if the serial is the NPC's backpack.
            if ( serial == cont->serial )
              return cont;

            // ... it is legal if the serial is a container inside the NPC's backpack.
            cont = cont->find_container( serial );
            if ( cont )
              return cont;

            cont = nullptr;
          }
        }
      }
    }
  }

  Item* item =
      chr->search_remote_containers( serial, nullptr /* don't care if it's a remote container */ );
  if ( item != nullptr && item->isa( UOBJ_CLASS::CLASS_CONTAINER ) )
    return static_cast<UContainer*>( item );
  else
    return nullptr;
}

Item* find_snoopable_item( u32 serial, Character** pchr )
{
  Item* item = system_find_item( serial );
  if ( item != nullptr )
  {
    Character* owner = item->GetCharacterOwner();
    if ( owner != nullptr )
    {
      if ( pchr != nullptr )
      {
        *pchr = owner;
      }
      return item;
    }
  }
  return nullptr;
}

// assume if you pass additlegal or isRemoteContainer, you init to false
Item* find_legal_item( const Character* chr, u32 serial, bool* additlegal, bool* isRemoteContainer )
{
  UContainer* backpack = chr->backpack();
  if ( backpack != nullptr && backpack->serial == serial )
    return backpack;

  // check worn items
  // 04/2007 - MuadDib Added:
  // find_wornitem will now check inside containers listed in layers
  // for normal items now also. This will allow for quivers
  // in wornitems, handbags, pockets, whatever people want,
  // to find stuff as a legal item to the character. Treats it
  // just like the backpack, without making it specific like
  // a bankbox or backpack.
  Item* item = chr->find_wornitem( serial );
  if ( item != nullptr )
    return item;

  if ( backpack != nullptr )
  {
    item = backpack->find( serial );
    if ( item != nullptr )
      return item;
  }

  // check items on the ground
  Range2d gridarea( zone_convert( chr->pos() - Vec2d( 8, 8 ) ),
                    zone_convert( chr->pos() + Vec2d( 8, 8 ) ), nullptr );
  for ( const auto& gpos : gridarea )
  {
    for ( const auto& _item : chr->realm()->getzone_grid( gpos ).items )
    {
      if ( !chr->in_visual_range( _item ) )
        continue;
      if ( _item->serial == serial )
      {
        passert_always( _item->container == nullptr );
        return _item;
      }
      if ( _item->isa( UOBJ_CLASS::CLASS_CONTAINER ) )
      {
        item = ( (const UContainer*)_item )->find( serial );
        if ( item != nullptr )
          return item;
      }
    }
  }

  if ( settingsManager.ssopt.master_can_access_npcs_backpack )
  {
    item = system_find_item( serial );
    if ( item )
    {
      // If the item is owned by by an NPC ...
      if ( auto owner = item->GetCharacterOwner(); owner && owner->isa( UOBJ_CLASS::CLASS_NPC ) )
      {
        auto npc = static_cast<Mobile::NPC*>( owner );

        // ... and chr is npc's master, and the item is either the backpack or something inside the
        // backpack (ie. not equipped) ...
        if ( npc->master() == chr && ( npc->backpack() == item || !npc->is_equipped( item ) ) )
        {
          // ... the item is legal.
          return item;
        }
      }
    }
  }

  if ( additlegal != nullptr )
    *additlegal = true;
  return chr->search_remote_containers( serial, isRemoteContainer );
}

void play_sound_effect( const UObject* center, u16 effect )
{
  Network::PlaySoundPkt msg( PKTOUT_54_FLAG_SINGLEPLAY, effect - 1u,
                             Pos3d( center->toplevel_pos().xy(), 0 ) );
  // FIXME hearing range check perhaps?
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( center,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr->in_visual_range( center ) )
                                                           msg.Send( zonechr->client );
                                                       } );
}

void play_sound_effect_xyz( const Pos4d& center, u16 effect )
{
  Network::PlaySoundPkt msg( PKTOUT_54_FLAG_SINGLEPLAY, effect - 1u, center.xyz() );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      center,
      [&]( Character* zonechr )
      {
        if ( zonechr->in_visual_range( nullptr, center ) )
          msg.Send( zonechr->client );
      } );
}

void play_sound_effect_private( const UObject* center, u16 effect, Character* forchr )
{
  if ( forchr->client )
  {
    Network::PlaySoundPkt msg( PKTOUT_54_FLAG_SINGLEPLAY, effect - 1u,
                               Pos3d( center->pos3d() ).z( 0 ) );
    msg.Send( forchr->client );
  }
}

void play_moving_effect( const UObject* src, const UObject* dst, u16 effect, u8 speed, u8 loop,
                         u8 explode )
{
  Network::GraphicEffectPkt msg;
  msg.movingEffect( src, dst, effect, speed, loop, explode );

  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( src,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr->in_visual_range( src ) )
                                                           msg.Send( zonechr->client );
                                                       } );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      dst,
      [&]( Character* zonechr )
      {
        if ( zonechr->in_visual_range( dst ) &&
             !zonechr->in_visual_range( src ) )  // send to char only in range of dst
          msg.Send( zonechr->client );
      } );
}

void play_moving_effect2( const Pos3d& src, const Pos3d& dst, u16 effect, u8 speed, u8 loop,
                          u8 explode, Realms::Realm* realm )
{
  Network::GraphicEffectPkt msg;
  msg.movingEffect( src, dst, effect, speed, loop, explode );

  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      src.xy(), realm,
      [&]( Character* zonechr )
      {
        if ( zonechr->in_visual_range( nullptr, src.xy() ) )
          msg.Send( zonechr->client );
      } );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      dst.xy(), realm,
      [&]( Character* zonechr )
      {
        if ( zonechr->in_visual_range( nullptr, dst.xy() ) &&
             !zonechr->in_visual_range( nullptr, src.xy() ) )  // send to chrs only in range of dest
          msg.Send( zonechr->client );
      } );
}


void play_lightning_bolt_effect( const UObject* center )
{
  Network::GraphicEffectPkt msg;
  msg.lightningBold( center );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( center,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr->in_visual_range( center ) )
                                                           msg.Send( zonechr->client );
                                                       } );
}

void play_object_centered_effect( const UObject* center, u16 effect, u8 speed, u8 loop )
{
  Network::GraphicEffectPkt msg;
  msg.followEffect( center, effect, speed, loop );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( center,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr->in_visual_range( center ) )
                                                           msg.Send( zonechr->client );
                                                       } );
}

void play_stationary_effect( const Pos4d& pos, u16 effect, u8 speed, u8 loop, u8 explode )
{
  Network::GraphicEffectPkt msg;
  msg.stationaryEffect( pos.xyz(), effect, speed, loop, explode );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      pos,
      [&]( Character* zonechr )
      {
        if ( zonechr->in_visual_range( nullptr, pos ) )
          msg.Send( zonechr->client );
      } );
}

void play_stationary_effect_ex( const Pos4d& pos, u16 effect, u8 speed, u8 duration, u32 hue,
                                u32 render, u16 effect3d )
{
  Network::GraphicEffectExPkt msg;
  msg.stationaryEffect( pos.xyz(), effect, speed, duration, hue, render, effect3d );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      pos,
      [&]( Character* zonechr )
      {
        if ( zonechr->in_visual_range( nullptr, pos ) )
          msg.Send( zonechr->client );
      } );
}

void play_object_centered_effect_ex( const UObject* center, u16 effect, u8 speed, u8 duration,
                                     u32 hue, u32 render, u8 layer, u16 effect3d )
{
  Network::GraphicEffectExPkt msg;
  msg.followEffect( center, effect, speed, duration, hue, render, layer, effect3d );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( center,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr->in_visual_range( center ) )
                                                           msg.Send( zonechr->client );
                                                       } );
}

void play_moving_effect_ex( const UObject* src, const UObject* dst, u16 effect, u8 speed,
                            u8 duration, u32 hue, u32 render, u8 direction, u8 explode,
                            u16 effect3d, u16 effect3dexplode, u16 effect3dsound )
{
  Network::GraphicEffectExPkt msg;
  msg.movingEffect( src, dst, effect, speed, duration, hue, render, direction, explode, effect3d,
                    effect3dexplode, effect3dsound );

  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( src,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr->in_visual_range( src ) )
                                                           msg.Send( zonechr->client );
                                                       } );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      dst,
      [&]( Character* zonechr ) {  // send to chrs only in range of dst
        if ( zonechr->in_visual_range( dst ) && !zonechr->in_visual_range( src ) )
          msg.Send( zonechr->client );
      } );
}

void play_moving_effect2_ex( const Pos3d& src, const Pos3d& dst, Realms::Realm* realm, u16 effect,
                             u8 speed, u8 duration, u32 hue, u32 render, u8 direction, u8 explode,
                             u16 effect3d, u16 effect3dexplode, u16 effect3dsound )
{
  Network::GraphicEffectExPkt msg;
  msg.movingEffect( src, dst, effect, speed, duration, hue, render, direction, explode, effect3d,
                    effect3dexplode, effect3dsound );

  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      src.xy(), realm,
      [&]( Character* zonechr )
      {
        if ( zonechr->in_visual_range( nullptr, src.xy() ) )
          msg.Send( zonechr->client );
      } );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      dst.xy(), realm,
      [&]( Character* zonechr )
      {
        if ( zonechr->in_visual_range( nullptr, dst.xy() ) &&
             !zonechr->in_visual_range( nullptr, src.xy() ) )  // send to chrs only in range of dst
          msg.Send( zonechr->client );
      } );
}

// System message -- message in lower left corner
void send_sysmessage( Network::Client* client, const char* text, unsigned short font,
                      unsigned short color )
{
  PktHelper::PacketOut<PktOut_1C> msg;
  std::string convertedText = Clib::strUtf8ToCp1252( text );
  u16 textlen = static_cast<u16>( convertedText.length() + 1 );
  if ( textlen > SPEECH_MAX_LEN + 1 )  // FIXME need to handle this better second msg?
    textlen = SPEECH_MAX_LEN + 1;

  msg->offset += 2;
  msg->Write<u32>( 0x01010101u );
  msg->Write<u16>( 0x0101u );
  msg->Write<u8>( Plib::TEXTTYPE_NORMAL );
  msg->WriteFlipped<u16>( color );
  msg->WriteFlipped<u16>( font );
  msg->Write( "System", 30 );
  msg->Write( convertedText.c_str(), textlen );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg.Send( client, len );
}

// Unicode System message -- message in lower left corner
void send_sysmessage_unicode( Network::Client* client, const std::string& text,
                              const std::string& lang, unsigned short font, unsigned short color )
{
  std::vector<u16> utf16text = Bscript::String::toUTF16( text );
  if ( utf16text.size() > SPEECH_MAX_LEN )
    utf16text.resize( SPEECH_MAX_LEN );
  PktHelper::PacketOut<PktOut_AE> msg;
  msg->offset += 2;
  msg->Write<u32>( 0x01010101u );
  msg->Write<u16>( 0x0101u );
  msg->Write<u8>( Plib::TEXTTYPE_NORMAL );
  msg->WriteFlipped<u16>( color );
  msg->WriteFlipped<u16>( font );
  msg->Write( lang.c_str(), 4 );
  msg->Write( "System", 30 );
  msg->WriteFlipped( utf16text );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg.Send( client, len );
}

void send_sysmessage( Network::Client* client, const std::string& text, unsigned short font,
                      unsigned short color )
{
  send_sysmessage( client, text.c_str(), font, color );
}

void broadcast( const char* text, unsigned short font, unsigned short color,
                unsigned short requiredCmdLevel )
{
  for ( auto& client : networkManager.clients )
  {
    if ( !client->ready || client->chr->cmdlevel_ < requiredCmdLevel )
      continue;

    send_sysmessage( client, text, font, color );
  }
}

void broadcast_unicode( const std::string& text, const std::string& lang, unsigned short font,
                        unsigned short color, unsigned short requiredCmdLevel )
{
  for ( auto& client : networkManager.clients )
  {
    if ( !client->ready || client->chr->cmdlevel_ < requiredCmdLevel )
      continue;

    send_sysmessage_unicode( client, text, lang, font, color );
  }
}

void send_nametext( Client* client, const Character* chr, const std::string& str )
{
  PktHelper::PacketOut<PktOut_1C> msg;
  std::string convertedString = Clib::strUtf8ToCp1252( str );
  u16 textlen = static_cast<u16>( convertedString.length() + 1 );
  if ( textlen > SPEECH_MAX_LEN + 1 )
    textlen = SPEECH_MAX_LEN + 1;

  msg->offset += 2;
  msg->Write<u32>( chr->serial_ext );
  msg->Write<u16>( 0x0101u );
  msg->Write<u8>( Plib::TEXTTYPE_YOU_SEE );
  msg->WriteFlipped<u16>( chr->name_color( client->chr ) );  // 0x03B2
  msg->WriteFlipped<u16>( 3u );
  msg->Write( convertedString.c_str(), 30 );
  msg->Write( convertedString.c_str(), textlen );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg.Send( client, len );
}

bool say_above( const UObject* obj, const char* text, unsigned short font, unsigned short color,
                unsigned int journal_print )
{
  PktHelper::PacketOut<PktOut_1C> msg;
  std::string convertedText = Clib::strUtf8ToCp1252( text );
  u16 textlen = static_cast<u16>( convertedText.length() + 1 );
  if ( textlen > SPEECH_MAX_LEN + 1 )  // FIXME need to handle this better second msg?
    textlen = SPEECH_MAX_LEN + 1;

  msg->offset += 2;
  msg->Write<u32>( obj->serial_ext );
  msg->WriteFlipped<u16>( obj->graphic );
  msg->Write<u8>( Plib::TEXTTYPE_NORMAL );
  msg->WriteFlipped<u16>( color );
  msg->WriteFlipped<u16>( font );
  switch ( journal_print )
  {
  case JOURNAL_PRINT_YOU_SEE:
    msg->Write( "You see", 30 );
    break;
  case JOURNAL_PRINT_NAME:
  default:
    msg->Write( Clib::strUtf8ToCp1252( obj->description() ).c_str(), 30 );
    break;
  }
  msg->Write( convertedText.c_str(), textlen );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  // todo: only send to those that I'm visible to.
  transmit_to_inrange( obj, &msg->buffer, len );
  return true;
}

bool say_above_unicode( const UObject* obj, const std::string& text, const std::string& lang,
                        unsigned short font, unsigned short color, unsigned int journal_print )
{
  std::vector<u16> utf16text = Bscript::String::toUTF16( text );
  if ( utf16text.size() > SPEECH_MAX_LEN )
    utf16text.resize( SPEECH_MAX_LEN );
  PktHelper::PacketOut<PktOut_AE> msg;
  msg->offset += 2;
  msg->Write<u32>( obj->serial_ext );
  msg->WriteFlipped<u16>( obj->graphic );
  msg->Write<u8>( Plib::TEXTTYPE_NORMAL );
  msg->WriteFlipped<u16>( color );
  msg->WriteFlipped<u16>( font );
  msg->Write( lang.c_str(), 4 );
  switch ( journal_print )
  {
  case JOURNAL_PRINT_YOU_SEE:
    msg->Write( "You see", 30 );
    break;
  case JOURNAL_PRINT_NAME:
  default:
    msg->Write( Clib::strUtf8ToCp1252( obj->description() ).c_str(), 30 );
    break;
  }
  msg->WriteFlipped( utf16text );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  // todo: only send to those that I'm visible to.
  transmit_to_inrange( obj, &msg->buffer, len );
  return true;
}

bool private_say_above( Character* chr, const UObject* obj, const char* text, unsigned short font,
                        unsigned short color, unsigned int journal_print )
{
  if ( chr->client == nullptr )
    return false;
  PktHelper::PacketOut<PktOut_1C> msg;
  std::string convertedText = Clib::strUtf8ToCp1252( text );
  u16 textlen = static_cast<u16>( convertedText.length() + 1 );
  if ( textlen > SPEECH_MAX_LEN + 1 )  // FIXME need to handle this better second msg?
    textlen = SPEECH_MAX_LEN + 1;

  msg->offset += 2;
  msg->Write<u32>( obj->serial_ext );
  msg->WriteFlipped<u16>( obj->graphic );
  msg->Write<u8>( Plib::TEXTTYPE_NORMAL );
  msg->WriteFlipped<u16>( color );
  msg->WriteFlipped<u16>( font );
  switch ( journal_print )
  {
  case JOURNAL_PRINT_YOU_SEE:
    msg->Write( "You see", 30 );
    break;
  case JOURNAL_PRINT_NAME:
  default:
    msg->Write( Clib::strUtf8ToCp1252( obj->description() ).c_str(), 30 );
    break;
  }
  msg->Write( convertedText.c_str(), textlen );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg.Send( chr->client, len );
  return true;
}

bool private_say_above_unicode( Character* chr, const UObject* obj, const std::string& text,
                                const std::string& lang, unsigned short font, unsigned short color,
                                unsigned int journal_print )
{
  std::vector<u16> utf16text = Bscript::String::toUTF16( text );
  if ( utf16text.size() > SPEECH_MAX_LEN )
    utf16text.resize( SPEECH_MAX_LEN );
  if ( chr->client == nullptr )
    return false;

  PktHelper::PacketOut<PktOut_AE> msg;
  msg->offset += 2;
  msg->Write<u32>( obj->serial_ext );
  msg->WriteFlipped<u16>( obj->graphic );
  msg->Write<u8>( Plib::TEXTTYPE_NORMAL );
  msg->WriteFlipped<u16>( color );
  msg->WriteFlipped<u16>( font );
  msg->Write( lang.c_str(), 4 );
  switch ( journal_print )
  {
  case JOURNAL_PRINT_YOU_SEE:
    msg->Write( "You see", 30 );
    break;
  case JOURNAL_PRINT_NAME:
  default:
    msg->Write( Clib::strUtf8ToCp1252( obj->description() ).c_str(), 30 );
    break;
  }
  msg->WriteFlipped( utf16text );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg.Send( chr->client, len );
  return true;
}

bool private_say_above_ex( Character* chr, const UObject* obj, const char* text,
                           unsigned short color )
{
  if ( chr->client == nullptr )
    return false;
  PktHelper::PacketOut<PktOut_1C> msg;
  std::string convertedText = Clib::strUtf8ToCp1252( text );
  u16 textlen = static_cast<u16>( convertedText.length() + 1 );
  if ( textlen > SPEECH_MAX_LEN + 1 )  // FIXME need to handle this better second msg?
    textlen = SPEECH_MAX_LEN + 1;

  msg->offset += 2;
  msg->Write<u32>( obj->serial_ext );
  msg->WriteFlipped<u16>( obj->graphic );
  msg->Write<u8>( Plib::TEXTTYPE_NORMAL );
  msg->WriteFlipped<u16>( color );
  msg->WriteFlipped<u16>( 3u );
  msg->Write( Clib::strUtf8ToCp1252( obj->description() ).c_str(), 30 );
  msg->Write( convertedText.c_str(), textlen );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg.Send( chr->client, len );
  return true;
}

void send_objdesc( Client* client, Item* item )
{
  PktHelper::PacketOut<PktOut_1C> msg;
  std::string convertedText = Clib::strUtf8ToCp1252( item->description() );
  u16 textlen = static_cast<u16>( convertedText.length() + 1 );
  if ( textlen > SPEECH_MAX_LEN + 1 )  // FIXME need to handle this better second msg?
    textlen = SPEECH_MAX_LEN + 1;
  msg->offset += 2;
  msg->Write<u32>( item->serial_ext );
  msg->WriteFlipped<u16>( item->graphic );
  msg->Write<u8>( Plib::TEXTTYPE_YOU_SEE );
  msg->WriteFlipped<u16>( 0x03B2u );
  msg->WriteFlipped<u16>( 3u );
  msg->Write( "System", 30 );
  msg->Write( convertedText.c_str(), textlen );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg.Send( client, len );
}

void send_stamina_level( Client* client )
{
  Character* chr = client->chr;

  PktHelper::PacketOut<PktOut_A3> msg;
  msg->Write<u32>( chr->serial_ext );

  if ( networkManager.uoclient_general.stamina.any )
  {
    int v = chr->vital( networkManager.uoclient_general.mana.id ).maximum_ones();
    if ( v > 0xFFFF )
      v = 0xFFFF;
    msg->WriteFlipped<u16>( static_cast<u16>( v ) );

    v = chr->vital( networkManager.uoclient_general.mana.id ).current_ones();
    if ( v > 0xFFFF )
      v = 0xFFFF;
    msg->WriteFlipped<u16>( static_cast<u16>( v ) );
  }
  else
  {
    msg->offset += 4;
  }
  msg.Send( client );
}

void send_mana_level( Client* client )
{
  Character* chr = client->chr;

  PktHelper::PacketOut<PktOut_A2> msg;
  msg->Write<u32>( chr->serial_ext );

  if ( networkManager.uoclient_general.mana.any )
  {
    int v = chr->vital( networkManager.uoclient_general.mana.id ).maximum_ones();
    if ( v > 0xFFFF )
      v = 0xFFFF;
    msg->WriteFlipped<u16>( static_cast<u16>( v ) );

    v = chr->vital( networkManager.uoclient_general.mana.id ).current_ones();
    if ( v > 0xFFFF )
      v = 0xFFFF;
    msg->WriteFlipped<u16>( static_cast<u16>( v ) );
  }
  else
  {
    msg->offset += 4;
  }
  msg.Send( client );
}

void send_death_message( Character* chr_died, Item* corpse )
{
  PktHelper::PacketOut<PktOut_AF> msg;
  msg->Write<u32>( chr_died->serial_ext );
  msg->Write<u32>( corpse->serial_ext );
  msg->offset += 4;  // u32 unk4_zero

  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( corpse,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr == chr_died )
                                                           return;
                                                         if ( zonechr->in_visual_range( corpse ) )
                                                           msg.Send( zonechr->client );
                                                       } );
}

void transmit_to_inrange( const UObject* center, const void* msg, unsigned msglen )
{
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      center,
      [&]( Character* zonechr )
      {
        if ( zonechr->in_visual_range( center ) )
          Core::networkManager.clientTransmit->AddToQueue( zonechr->client, msg, msglen );
      } );
}

void transmit_to_others_inrange( Character* center, const void* msg, unsigned msglen )
{
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      center,
      [&]( Character* zonechr )
      {
        Client* client = zonechr->client;
        if ( zonechr == center )
          return;
        if ( zonechr->in_visual_range( center ) )
          Core::networkManager.clientTransmit->AddToQueue( client, msg, msglen );
      } );
}

void destroy_item( Item* item )
{
  if ( item->serial == 0 )
  {
    POLLOG_ERRORLN( "destroy {}: {}, orphan! (old serial: {:#x})", item->description(),
                    item->classname(), ( cfBEu32( item->serial_ext ) ) );
  }

  if ( item->serial != 0 )
  {
    /*
        cout << "destroy " << item->description() << ": "
        << item->classname() << " " <<  item
        << ", serial=" << hexint(item->serial) << endl;
        */
    item->set_dirty();

    send_remove_object_to_inrange( item );

    if ( item->container == nullptr )  // on ground, easy.
    {
      if ( !item->has_gotten_by() )  // and not in hand
        remove_item_from_world( item );
    }
    else
    {
      item->extricate();
    }

    item->destroy();
  }
}

void setrealm( Item* item, void* arg )
{
  Realms::Realm* realm = static_cast<Realms::Realm*>( arg );
  item->setposition( Pos4d( item->pos().xyz(), realm ) );
}

void setrealmif( Item* item, void* arg )
{
  Realms::Realm* realm = static_cast<Realms::Realm*>( arg );
  if ( item->realm() == realm )
    item->setposition( Pos4d( item->pos().xyz(), realm->baserealm ) );
}

void subtract_amount_from_item( Item* item, unsigned short amount )
{
  if ( amount >= item->getamount() )
  {
    destroy_item( item );
    return;  // destroy_item will update character weight if item is carried.
  }
  else
  {
    item->subamount( amount );
    update_item_to_inrange( item );
  }
  // DAVE added this 11/17: if in a Character's pack, update weight.
  UpdateCharacterWeight( item );
}


// FIXME OPTIMIZE: Core is building the packet in send_item for every single client
// that needs to get it. There should be a better method for this. Such as, a function
// to run all the checks after building the packet here, then send as it needs to.
void move_item( Items::Item* item, const Core::Pos4d& oldpos )
{
  item->restart_decay_timer();
  MoveItemWorldPosition( oldpos, item );

  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( item,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr->in_visual_range( item ) )
                                                           send_item( zonechr->client, item );
                                                       } );
  Network::RemoveObjectPkt msgremove( item->serial_ext );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      oldpos,
      [&]( Character* zonechr )
      {
        if ( !zonechr->in_visual_range( item, oldpos ) )
          return;
        if ( !zonechr->in_visual_range(
                 item ) )  // not in range.  If old loc was in range, send a delete.
          msgremove.Send( zonechr->client );
      } );
}

void send_multi( Client* client, const Multi::UMulti* multi )
{
  auto pkt =
      SendWorldMulti( multi->serial_ext, multi->multidef().multiid, multi->pos3d(), multi->color );
  pkt.Send( client );
}

void send_multi_to_inrange( const Multi::UMulti* multi )
{
  auto pkt =
      SendWorldMulti( multi->serial_ext, multi->multidef().multiid, multi->pos3d(), multi->color );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( multi,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr->in_visual_range( multi ) )
                                                           pkt.Send( zonechr->client );
                                                       } );
}


void update_lightregion( Client* client, LightRegion* /*lightregion*/ )
{
  if ( !client->ready )
    return;

  client->chr->check_light_region_change();
}

void SetRegionLightLevel( LightRegion* lightregion, int lightlevel )
{
  lightregion->lightlevel = lightlevel;
  PktHelper::PacketOut<PktOut_4F> msg;
  msg->Write<u8>( static_cast<u8>( lightlevel ) );
  for ( Clients::iterator itr = networkManager.clients.begin(), end = networkManager.clients.end();
        itr != end; ++itr )
  {
    Client* client = *itr;
    if ( !client->ready )
      continue;

    auto light_until = client->chr->lightoverride_until();
    if ( light_until < read_gameclock() && light_until != ~0u )
    {
      client->chr->lightoverride_until( 0 );
      client->chr->lightoverride( -1 );
    }

    if ( client->gd->weather_region && client->gd->weather_region->lightoverride != -1 &&
         !client->chr->has_lightoverride() )
      continue;

    int newlightlevel;
    if ( client->chr->has_lightoverride() )
      newlightlevel = client->chr->lightoverride();
    else
    {
      // dave 12-22 check for no regions
      LightRegion* light_region = gamestate.lightdef->getregion( client->chr->pos() );
      if ( light_region != nullptr )
        newlightlevel = light_region->lightlevel;
      else
        newlightlevel = settingsManager.ssopt.default_light_level;
    }

    if ( newlightlevel != client->gd->lightlevel )
    {
      if ( VALID_LIGHTLEVEL( newlightlevel ) )
      {
        if ( newlightlevel != lightlevel )
        {
          msg->offset = 1;
          msg->Write<u8>( static_cast<u8>( newlightlevel ) );
          msg.Send( client );
          msg->offset = 1;
          msg->Write<u8>( static_cast<u16>( lightlevel ) );
        }
        else
          msg.Send( client );
      }
      client->gd->lightlevel = newlightlevel;
    }
  }
}

void update_weatherregion( Client* client, WeatherRegion* weatherregion )
{
  if ( !client->ready )
    return;

  if ( client->gd->weather_region == weatherregion )
  {
    // client->gd->weather_region = nullptr;  //dave commented this out 5/26/03, causing no
    // processing to happen in following function, added force bool instead.
    client->chr->check_weather_region_change( true );
    client->chr->check_light_region_change();
  }
}

void SetRegionWeatherLevel( WeatherRegion* weatherregion, unsigned type, unsigned severity,
                            unsigned aux, int lightoverride )
{
  weatherregion->weathertype = static_cast<unsigned char>( type );
  weatherregion->severity = static_cast<unsigned char>( severity );
  weatherregion->aux = static_cast<unsigned char>( aux );
  weatherregion->lightoverride = lightoverride;

  for ( auto& client : networkManager.clients )
  {
    update_weatherregion( client, weatherregion );
  }
}

void update_all_weatherregions()
{
  for ( auto& client : networkManager.clients )
  {
    if ( !client->ready )
      return;

    client->chr->check_weather_region_change();
    client->chr->check_light_region_change();
  }
}

/* there are four forms of 'name' in objinfo:
    name              (normal)
    name%s            (percent followed by plural-part, then null-term)
    name%s%           (percent followed by plural-part, then percent, then more)
    wheat shea%ves/f% ( '%', plural part, '/', single part, '%', rest )
    Some examples:
    pil%es/e% of hides
    banana%s%
    feather%s
    Known bugs:
    1 gold coin displays as "gold coin".  There must be a bit somewhere
    that I just don't understand yet.
    */
std::string format_description( unsigned int polflags, const std::string& descdef,
                                unsigned short amount, const std::string suffix )
{
  std::string desc;

  if ( amount != 1 )
  {
    char s[15];
    snprintf( s, Clib::arsize( s ), "%hu ", amount );
    desc = s;
  }
  else if ( settingsManager.ssopt.use_tile_flag_prefix )
  {
    if ( polflags & Plib::FLAG::DESC_PREPEND_AN )
    {
      desc = "an ";
    }
    else if ( polflags & Plib::FLAG::DESC_PREPEND_A )
    {
      desc = "a ";
    }
  }

  // might want to consider strchr'ing here,
  //   if not found, strcpy/return
  //   if found, memcpy up to strchr result, then continue as below.
  const char* src = descdef.c_str();
  int singular = ( amount == 1 );
  int plural_handled = 0;
  int phase = 0; /* 0= first part, 1=plural part, 2=singular part, 3=rest */
  char ch;
  while ( '\0' != ( ch = *src ) )
  {
    if ( phase == 0 )
    {
      if ( ch == '%' )
      {
        plural_handled = 1;
        phase = 1;
      }
      else
      {
        desc += ch;
      }
    }
    else if ( phase == 1 )
    {
      if ( ch == '%' )
        phase = 3;
      else if ( ch == '/' )
        phase = 2;
      else if ( !singular )
        desc += ch;
    }
    else if ( phase == 2 )
    {
      if ( ch == '%' )
        phase = 3;
      else if ( singular )
        desc += ch;
    }
    // if phase == 3 that means there are more words to come,
    // lets loop through them to support singular/plural stuff in more than just the first word of
    // the desc.
    else
    {
      desc += ch;
      phase = 0;
    }
    ++src;
  }

  if ( !singular && !plural_handled )
    desc += 's';

  if ( !suffix.empty() )
    desc += " " + suffix;

  return desc;
}

void send_midi( Client* client, u16 midi )
{
  PktHelper::PacketOut<PktOut_6D> msg;
  msg->WriteFlipped<u16>( midi );
  msg.Send( client );
  // cout << "Setting midi for " << client->chr->name() << " to " << midi << endl;
}

void register_with_supporting_multi( Item* item )
{
  if ( item->container == nullptr )
  {
    Multi::UMulti* multi = item->realm()->find_supporting_multi( item->pos3d() );
    if ( multi )
      multi->register_object( item );
  }
}


void send_create_mobile_if_nearby_cansee( Client* client, const Character* chr )
{
  if ( client->ready &&  // must be logged into game
       client->chr != chr && client->chr->is_visible_to_me( chr ) )
  {
    send_owncreate( client, chr );
  }
}

void send_create_mobile_to_nearby_cansee( const Character* chr )
{
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange( chr,
                                                       [&]( Character* zonechr )
                                                       {
                                                         if ( zonechr == chr )
                                                           return;
                                                         if ( zonechr->is_visible_to_me( chr ) )
                                                           send_owncreate( zonechr->client, chr );
                                                       } );
}

void send_move_mobile_to_nearby_cansee( const Character* chr, bool send_health_bar_status_update )
{
  MoveChrPkt msgmove( chr );
  std::unique_ptr<HealthBarStatusUpdate> msgpoisoned;
  std::unique_ptr<HealthBarStatusUpdate> msginvul;
  if ( chr->poisoned() || send_health_bar_status_update )
    msgpoisoned.reset( new HealthBarStatusUpdate(
        chr->serial_ext, HealthBarStatusUpdate::Color::GREEN, chr->poisoned() ) );
  if ( chr->invul() || send_health_bar_status_update )
    msginvul.reset( new HealthBarStatusUpdate(
        chr->serial_ext, HealthBarStatusUpdate::Color::YELLOW, chr->invul() ) );
  WorldIterator<OnlinePlayerFilter>::InMaxVisualRange(
      chr,
      [&]( Character* zonechr )
      {
        if ( !send_health_bar_status_update && zonechr == chr )
          return;
        if ( zonechr->is_visible_to_me( chr ) )
        {
          msgmove.Send( zonechr->client );
          if ( msgpoisoned )
            msgpoisoned->Send( zonechr->client );
          if ( msginvul )
            msginvul->Send( zonechr->client );
        }
      } );
}

Character* UpdateCharacterWeight( Item* item )
{
  Character* chr_owner = item->GetCharacterOwner();
  if ( chr_owner != nullptr && chr_owner->client != nullptr )
  {
    send_full_statmsg( chr_owner->client, chr_owner );
    return chr_owner;
  }
  return nullptr;
}

void UpdateCharacterOnDestroyItem( Item* item )
{
  Character* chr_owner = item->GetCharacterOwner();
  if ( chr_owner != nullptr )
  {
    if ( item->layer && chr_owner->is_equipped( item ) )
    {
      item->check_unequiptest_scripts();
      item->check_unequip_script();
      send_remove_object_to_inrange( item );
    }
  }
}

// Dave added this 12/1/02
bool clientHasCharacter( Client* c )
{
  return ( c->chr != nullptr );
}

void login_complete( Client* c )
{
  PktHelper::PacketOut<PktOut_55> msg;
  msg.Send( c );
}

void send_feature_enable( Client* client )
{
  auto clientflag =
      client->acct->expansion().calculatedExtensionFlags( settingsManager.ssopt.features );

  PktHelper::PacketOut<PktOut_B9> msg;
  if ( client->ClientType & CLIENTTYPE_60142 )
    msg->WriteFlipped<u32>( static_cast<u32>( clientflag ) & 0xFFFFFFFFu );
  else
    msg->WriteFlipped<u16>( static_cast<u32>( clientflag ) & 0xFFFFu );
  msg.Send( client );
}

void send_realm_change( Client* client, Realms::Realm* realm )
{
  PktHelper::PacketOut<PktOut_BF_Sub8> msg;
  msg->WriteFlipped<u16>( 6u );
  msg->offset += 2;  // sub
  msg->Write<u8>( realm->getUOMapID() );
  msg.Send( client );
}

/// Sends number of maps used and number of map/static patches for each map
void send_map_difs( Client* client )
{
  // Prepare the data by reading map id used by every realm
  // When a map is used multiple times in different realms, only
  // take into account first realm found. No support for using the
  // same map with different diff files for different realms.
  struct mapdiff
  {
    u32 static_patches;
    u32 map_patches;
  };
  std::map<u32, mapdiff> mapinfo;
  for ( auto it = gamestate.Realms.begin(); it != gamestate.Realms.end(); ++it )
  {
    mapdiff md = { ( *it )->getNumStaticPatches(), ( *it )->getNumMapPatches() };
    mapinfo.insert( std::pair<u32, mapdiff>( ( *it )->getUOMapID(), md ) );
  }

  u32 max_map_id = mapinfo.rbegin()->first;

  PktHelper::PacketOut<PktOut_BF_Sub18> msg;
  msg->offset += 4;                          // len+sub
  msg->WriteFlipped<u32>( max_map_id + 1 );  // Number of maps
  for ( u32 i = 0; i <= max_map_id; i++ )
  {
    auto it = mapinfo.find( i );
    if ( it == mapinfo.end() )
    {
      // Filling hole (map not used)
      msg->WriteFlipped<u32>( 0u );
      msg->WriteFlipped<u32>( 0u );
    }
    else
    {
      msg->WriteFlipped<u32>( mapinfo.at( i ).static_patches );
      msg->WriteFlipped<u32>( mapinfo.at( i ).map_patches );
    }
  }
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg.Send( client, len );
}

// FIXME : Works, except for Login. Added length check as to not mess with 1.x clients
//         during login. Added send_season_info to handle_client_version to make up.
void send_season_info( Client* client )
{
  if ( client->getversiondetail().major >= 1 )
  {
    PktHelper::PacketOut<PktOut_BC> msg;
    msg->Write<u8>( client->chr->realm()->season() );
    msg->Write<u8>( PKTOUT_BC::PLAYSOUND_YES );
    msg.Send( client );

    // Sending Season info resets light level in client, this fixes it during login
    if ( client->gd->weather_region != nullptr && client->gd->weather_region->lightoverride != -1 &&
         !client->chr->has_lightoverride() )
    {
      send_light( client, client->gd->weather_region->lightoverride );
    }
  }
}

// assumes new realm has been set on client->chr
void send_new_subserver( Client* client )
{
  PktHelper::PacketOut<PktOut_76> msg;
  msg->WriteFlipped<u16>( client->chr->x() );
  msg->WriteFlipped<u16>( client->chr->y() );
  msg->WriteFlipped<u16>( static_cast<u16>( client->chr->z() ) );
  msg->offset += 5;  // unk0,x1,y2
  msg->WriteFlipped<u16>( client->chr->realm()->width() );
  msg->WriteFlipped<u16>( client->chr->realm()->height() );
  msg.Send( client );
}

void send_fight_occuring( Client* client, Character* opponent )
{
  PktHelper::PacketOut<PktOut_2F> msg;
  msg->offset++;  // zero1
  msg->Write<u32>( client->chr->serial_ext );
  msg->Write<u32>( opponent->serial_ext );
  msg.Send( client );
}

void send_damage( Character* attacker, Character* defender, u16 damage )
{
  SendDamagePkt pkt( defender->serial_ext, damage );
  if ( attacker->client != nullptr )
    pkt.Send( attacker->client );
  if ( ( defender->client != nullptr ) && ( attacker != defender ) )
    pkt.Send( defender->client );
}

void sendCharProfile( Character* chr, Character* of_who, const std::string& title,
                      const std::string& utext, const std::string& etext )
{
  PktHelper::PacketOut<PktOut_B8> msg;
  std::vector<u16> uwtext = Bscript::String::toUTF16( utext );
  std::vector<u16> ewtext = Bscript::String::toUTF16( etext );

  std::string convertedText = Clib::strUtf8ToCp1252( title );
  size_t titlelen = convertedText.length() + 1;
  // Check Lengths
  if ( titlelen > SPEECH_MAX_LEN )
    titlelen = SPEECH_MAX_LEN;
  if ( uwtext.size() > SPEECH_MAX_LEN )
    uwtext.resize( SPEECH_MAX_LEN );
  if ( ewtext.size() > SPEECH_MAX_LEN )
    ewtext.resize( SPEECH_MAX_LEN );

  // Build Packet
  msg->offset += 2;
  msg->Write<u32>( of_who->serial_ext );
  msg->Write( convertedText.c_str(), static_cast<u16>( titlelen ) );
  msg->WriteFlipped( uwtext );
  msg->WriteFlipped( ewtext );
  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );
  msg.Send( chr->client, len );
}

/**
 * Sends the packet for the buff bar
 * @author Bodom, 2015-12-02
 *
 * @param chr the Character to send the packet to
 * @param icon the ID of the icon to show/remove
 * @param show if true, shows/updates the icon; if false, removes the icon
 * @param duration duration in seconds, only for displaying [ignored if show is false]
 * @param cl_name name of the buff, cliloc id [ignored if show is false]
 * @param cl_descr description of the buff, cliloc id [ignored if show is false]
 * @param arguments arguments for cl_descr as string, separated by spaces
 */
void send_buff_message( Character* chr, u16 icon, bool show, u16 duration, u32 cl_name,
                        const std::string& name_arguments, u32 cl_descr,
                        const std::string& desc_arguments )
{
  PktHelper::PacketOut<PktOut_DF> msg;
  msg->offset += 2;  // length will be written later
  msg->Write<u32>( chr->serial_ext );
  msg->WriteFlipped<u16>( icon );
  msg->WriteFlipped<u16>( show ? 1u : 0u );
  if ( show )
  {
    msg->Write<u32>( 0u );  // unknown, always 0
    msg->WriteFlipped<u16>( icon );
    msg->WriteFlipped<u16>( 1u );  // unknown, always 1
    msg->Write<u32>( 0u );         // unknown, always 0
    msg->WriteFlipped<u16>( duration );
    msg->Write<u16>( 0u );  // unknown, always 0
    msg->Write<u8>( 0u );   // unknown, always 0
    msg->WriteFlipped<u32>( cl_name );
    msg->WriteFlipped<u32>( cl_descr );
    msg->Write<u32>( 0u );  // 3rd cliloc?
    auto nameargs = Bscript::String::toUTF16( name_arguments );
    msg->WriteFlipped<u16>( nameargs.size() + 1 );
    msg->Write( nameargs );
    auto descargs = Bscript::String::toUTF16( desc_arguments );
    msg->WriteFlipped<u16>( descargs.size() + 1 );
    msg->Write( descargs );
    msg->WriteFlipped<u16>( 1u );  // 3rd arg length?
    msg->Write<u16>( 0u );
  }

  u16 len = msg->offset;
  msg->offset = 1;
  msg->WriteFlipped<u16>( len );

  msg.Send( chr->client, len );
}
}  // namespace Core
}  // namespace Pol
