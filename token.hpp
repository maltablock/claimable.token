/**
 *  @file
 *  @copyright defined in LICENSE
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/singleton.hpp>
#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   class token : public contract {
      public:
         token( account_name self ):contract(self){}

         void create( account_name issuer,
                      asset        maximum_supply);
         void update( account_name issuer,
                      asset        maximum_supply);
         void issue( account_name to, asset quantity, string memo );
         void claim( account_name owner, symbol_type sym );
         void signup( account_name owner, symbol_type sym );
         void recover( account_name owner, symbol_type sym );
         void transfer( account_name from,
                        account_name to,
                        asset        quantity,
                        string       memo );

         inline asset get_supply( symbol_name sym )const;
         inline asset get_balance( account_name owner, symbol_name sym )const;


      private:
         struct account {
            asset    balance;
            bool     claimed = false;
            uint64_t primary_key()const { return balance.symbol.name(); }
         };

         struct currency_stats {
            asset          supply;
            asset          max_supply;
            account_name   issuer;

            uint64_t primary_key()const { return supply.symbol.name(); }
         };

         struct st_signups {
           uint64_t count;
         };

         typedef eosio::multi_index<N(accounts), account> accounts;
         typedef eosio::multi_index<N(stat), currency_stats> stats;
         typedef eosio::singleton<N(signups), st_signups> signups;

         void sub_balance( account_name owner, asset value );
         void add_balance( account_name owner, asset value, account_name ram_payer, bool claimed );
         void do_claim( account_name owner, symbol_type sym, account_name payer );
      public:
         struct transfer_args {
            account_name  from;
            account_name  to;
            asset         quantity;
            string        memo;
         };
   };


   asset token::get_supply( symbol_name sym )const
   {
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );
      return st.supply;
   }

   asset token::get_balance( account_name owner, symbol_name sym )const
   {
      accounts accountstable( _self, owner );
      const auto& ac = accountstable.get( sym );
      return ac.balance;
   }

} /// namespace eosio
