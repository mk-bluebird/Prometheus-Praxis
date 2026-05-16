// filename: x/sabotage/keeper/msg_server_dependency_whitelist.go
// destination: eco_restoration_shard/x/sabotage/keeper/msg_server_dependency_whitelist.go
// repo-target: github.com/mk-bluebird/eco_restoration_shard

package keeper

import (
    "context"

    sdk "github.com/cosmos/cosmos-sdk/types"
    "github.com/mk-bluebird/eco_restoration_shard/x/sabotage/types"
)

func (k msgServer) SetDependencyWhitelist(
    goCtx context.Context,
    msg *types.MsgSetDependencyWhitelist,
) (*types.MsgSetDependencyWhitelistResponse, error) {
    ctx := sdk.UnwrapSDKContext(goCtx)

    host, err := sdk.AccAddressFromBech32(msg.BostromAddress)
    if err != nil {
        return nil, err
    }

    if err := k.whitelistStore.SetWhitelist(ctx, host, msg.MerkleRoot, msg.Versiontag); err != nil {
        return nil, err
    }

    return &types.MsgSetDependencyWhitelistResponse{}, nil
}
