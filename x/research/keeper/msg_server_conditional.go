// filename: x/research/keeper/msg_server_conditional.go
// destination: eco_restoration_shard/x/research/keeper/msg_server_conditional.go
// repo-target: github.com/mk-bluebird/eco_restoration_shard

package keeper

import (
    "context"

    sdk "github.com/cosmos/cosmos-sdk/types"
    "github.com/mk-bluebird/eco_restoration_shard/x/research/types"
)

func (k msgServer) ConditionalSubmitManifest(
    goCtx context.Context,
    msg *types.MsgConditionalSubmitManifest,
) (*types.MsgConditionalSubmitManifestResponse, error) {
    ctx := sdk.UnwrapSDKContext(goCtx)

    manifestID, err := k.pendingStore.SavePendingManifest(ctx, msg)
    if err != nil {
        return nil, err
    }

    return &types.MsgConditionalSubmitManifestResponse{
        ManifestId: manifestID,
    }, nil
}

func (k msgServer) ConfirmManifest(
    goCtx context.Context,
    msg *types.MsgConfirmManifest,
) (*types.MsgConfirmManifestResponse, error) {
    ctx := sdk.UnwrapSDKContext(goCtx)

    pending, found := k.pendingStore.GetPendingManifest(ctx, msg.ManifestId)
    if !found {
        return nil, types.ErrPendingManifestNotFound
    }

    if ctx.BlockHeight() > pending.ConfirmationDeadline {
        return nil, types.ErrConfirmationExpired
    }

    didEntry, err := k.didRegistry.GetHostDID(ctx, pending.BostromAddress)
    if err != nil {
        return nil, err
    }

    manifestHash := k.HashManifest(pending.Manifest)

    if !k.bciVerifier.Verify(
        didEntry.BciPublicKey,
        manifestHash,
        msg.ConfirmationSignature.Signature,
    ) {
        return nil, types.ErrInvalidHostConfirmation
    }

    if err := k.applyManifest(ctx, pending); err != nil {
        return nil, err
    }

    k.pendingStore.DeletePendingManifest(ctx, msg.ManifestId)

    return &types.MsgConfirmManifestResponse{}, nil
}
