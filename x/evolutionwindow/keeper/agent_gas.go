// filename: x/evolutionwindow/keeper/agent_gas.go
// destination: eco_restoration_shard/x/evolutionwindow/keeper/agent_gas.go
// repo-target: github.com/mk-bluebird/eco_restoration_shard

package keeper

import (
    sdk "github.com/cosmos/cosmos-sdk/types"
    "github.com/mk-bluebird/eco_restoration_shard/x/evolutionwindow/types"
)

func (k Keeper) BeforeAgentMessage(ctx sdk.Context, ticket types.CyberneticDelegationTicket, gasCost sdk.Gas) error {
    ticketID := ticket.Id
    gasUsed := k.getAgentGasUsed(ctx, ticketID)
    limit := ticket.AgentGasLimit

    newGas := gasUsed + uint64(gasCost)

    if newGas > limit {
        return types.ErrAgentGasLimitExceeded
    }

    k.setAgentGasUsed(ctx, ticketID, newGas)
    return nil
}

func (k Keeper) getAgentGasUsed(ctx sdk.Context, ticketID string) uint64 {
    store := ctx.KVStore(k.storeKey)
    bz := store.Get(agentGasKey(ticketID))
    if bz == nil {
        return 0
    }
    return sdk.BigEndianToUint64(bz)
}

func (k Keeper) setAgentGasUsed(ctx sdk.Context, ticketID string, value uint64) {
    store := ctx.KVStore(k.storeKey)
    store.Set(agentGasKey(ticketID), sdk.Uint64ToBigEndian(value))
}

func agentGasKey(ticketID string) []byte {
    return append([]byte("agent_gas_used/"), []byte(ticketID)...)
}
