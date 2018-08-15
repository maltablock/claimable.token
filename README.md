# claimable.token
Claimable EOSIO token to recover RAM quickly. Also allows you to change issuer and max supply.

# How does this token recover ram
Added to the balance table is the claimed boolean flag.

When the token is issued claimed = false and the RAM is paid by the issuer.

When the token is transferred the balance row is dropped and re-added using the RAM of the owner.

Tokens can also be explicitly claimed using the ```claim``` action, if the owner doesn't want to transfer.

In theory, unclaimed tokens can be ```recover```ed after some claiming period to have guaranteed RAM recovery.
