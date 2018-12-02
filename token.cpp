/**
 *  @file
 *  @copyright defined in LICENSE
 */

#include "token.hpp"

namespace eosio {

void token::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::update( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before update" );
    const auto& st = *existing;

    eosio_assert( st.supply.amount <= maximum_supply.amount, "max-supply cannot be less than available supply");
    eosio_assert( maximum_supply.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, 0, [&]( auto& s ) {
      s.max_supply    = maximum_supply;
      s.issuer        = issuer;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer, true );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    do_claim( from, quantity.symbol, from );
    sub_balance( from, quantity );
    add_balance( to, quantity, from, from != st.issuer );

    //account needs to exist first, dont auto claim when issuer
    if(from != st.issuer) {
      //eosio_assert( false, "transfers frozen during airdrop");
      do_claim( to, quantity.symbol, from );
    }

    if( to == _self) {
      do_swap( from, quantity );
    }
}

void token::do_swap( account_name from, asset quantity ) {
  //confirm quantity
  symbol_type burn_symbol = S(4,ZKSPLAY);
  symbol_type issue_symbol = S(0,ZKS);
  uint64_t issued = 0;
  uint64_t burned = 0;
  std::string memo = "Converted ";
  if(quantity.symbol == burn_symbol) {
    issued = quantity.amount / 1'0000;
    burned = issued * 1'0000;
    uint64_t surplus = quantity.amount - burned;
    //return the surplus ZKSPLAY
    if(surplus > 0)
      SEND_INLINE_ACTION( *this, transfer, {_self,N(active)}, {_self, from, asset(surplus,quantity.symbol), "Surplus ZKSPLAY from swapping to ZKS"} );
    memo += std::string("ZKSPLAY to ZKS");
  } else {
    burn_symbol = S(0,ZKS);
    issue_symbol = S(4,ZKSPLAY);
    issued = quantity.amount * 1'0000;
    memo += std::string("ZKS to ZKSPLAY");
  }
  eosio_assert(issued > 0, "Swap must be greater than 0");

  auto burned_asset = asset(burned,burn_symbol);
  sub_balance( _self, burned_asset );

  auto issued_asset = asset(issued,issue_symbol);
  add_balance( from, issued_asset, from, true );

  SEND_INLINE_ACTION( *this, conversion, {_self,N(active)}, {_self, from, issued_asset, memo} );
}

void token::conversion(account_name from, account_name to, asset quantity, string memo) {
  require_auth(_self);
  require_recipient( to );
  //does nothing except provide receipt
}

void token::open( account_name owner, symbol_type symbol, account_name ram_payer ) {
    require_auth( ram_payer );

    eosio_assert( symbol.is_valid(), "invalid symbol" );
    auto sym_name = symbol.name();

    stats statstable( _self, sym_name );
    const auto& st = statstable.get( sym_name, "symbol does not exist" );
    eosio_assert( st.supply.symbol == symbol, "symbol precision mismatch" );

    accounts owner_acnts( _self, owner );
    auto owned = owner_acnts.find( sym_name );
    if(owned == owner_acnts.end()) {
      add_balance( owner, asset(0,symbol), ram_payer, true );
    }
}

void token::claim( account_name owner, symbol_type sym ) {
  do_claim(owner,sym,owner);
}

void token::do_claim( account_name owner, symbol_type sym, account_name payer ) {
  eosio_assert( sym.is_valid(), "invalid symbol name" );
  auto sym_name = sym.name();

  require_auth( payer );
  accounts owner_acnts( _self, owner );

  const auto& existing = owner_acnts.get( sym_name, "no balance object found" );
  if( !existing.claimed ) {
    //save the balance
    auto value = existing.balance;
    //erase the table freeing ram to the issuer
    owner_acnts.erase( existing );
    //create a new index
    auto replace = owner_acnts.find( sym_name );
    //confirm there are definitely no balance now
    eosio_assert(replace == owner_acnts.end(), "there must be no balance object" );
    //add the new claimed balance paid by owner
    owner_acnts.emplace( payer, [&]( auto& a ){
      a.balance = value;
      a.claimed = true;
    });
  }
}

void token::recover( account_name owner, symbol_type sym ) {
  eosio_assert( sym.is_valid(), "invalid symbol name" );
  auto sym_name = sym.name();

  stats statstable( _self, sym_name );
  auto existing = statstable.find( sym_name );
  eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
  const auto& st = *existing;

  require_auth( st.issuer );

  //fail gracefully so we dont have to take another snapshot
  accounts owner_acnts( _self, owner );
  auto owned = owner_acnts.find( sym_name );
  if( owned != owner_acnts.end() ) {
    if( !owned->claimed ) {
      sub_balance( owner, owned->balance );
      add_balance( st.issuer, owned->balance, st.issuer, true );
    }
  }
}

void token::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void token::add_balance( account_name owner, asset value, account_name ram_payer, bool claimed )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
        a.claimed = claimed;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} /// namespace eosio

EOSIO_ABI( eosio::token, (create)(update)(issue)(transfer)(claim)(recover)(conversion)(open) )
