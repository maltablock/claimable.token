# claimable.token
Claimable EOSIO token to recover RAM quickly. Also allows you to change issuer and max supply.

# How does this token recover ram
Added to the balance table is the claimed boolean flag.

When the token is issued claimed = false and the RAM is paid by the issuer.

When the token is transferred the balance row is dropped and re-added using the RAM of the owner.

Tokens can also be explicitly claimed using the ```claim``` action, if the owner doesn't want to transfer.

In theory, unclaimed tokens can be ```recover```ed after some claiming period to have guaranteed RAM recovery.

# New functions

* update

This action uses the same ABI as create and allows you to change the issuer and/or max supply of a token

Only the token contract itself can authorize this action, not the previous issuer

```cleos push action claimable.token update '{"issuer":"newissuer","maximum_supply":"100000.0000 TOK"}'```

* claim

This action allows a token balance owner to claim their token without transferring it

This is only really neccessary if the token contract will recover unclaimed Tokens

```cleos push action claimable.token claim '{"owner":"user1","sym":"4,TOK"}'```

* recover

This action allows the token contract to recover unclaimed tokens, thus freeing RAM

The unclaimed tokens are added to the current issuer balance

This command only does something if the balance is truly unclaimed, otherwise it does nothing

```cleos push action claimable.token recover '{"owner":"user1","sym":"4,TOK"}'```
