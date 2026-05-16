// filename: x/research/keeper/msg_server_submit_manifest.go
// destination: eco_restoration_shard/x/research/keeper/msg_server_submit_manifest.go
// repo-target: github.com/mk-bluebird/eco_restoration_shard

package keeper

import (
    "context"

    sdk "github.com/cosmos/cosmos-sdk/types"
    "github.com/mk-bluebird/eco_restoration_shard/x/research/types"
)

func (k msgServer) SubmitManifest(ctx context.Context, msg *types.MsgSubmitManifest) (*types.MsgSubmitManifestResponse, error) {
    sdkCtx := sdk.UnwrapSDKContext(ctx)

    manifestHash := k.HashManifest(msg.Manifest)

    didEntry, err := k.didRegistry.GetHostDID(sdkCtx, msg.Creator)
    if err != nil {
        return nil, err
    }

    if !k.verifyBCIApproval(didEntry.BciPublicKey, manifestHash, msg.HostApprovalProof) {
        return nil, types.ErrInvalidHostApproval
    }

    // existing manifest processing logic follows
    return &types.MsgSubmitManifestResponse{}, nil
}

func (k Keeper) HashManifest(manifest types.ResearchManifest) []byte {
    bz := k.cdc.MustMarshal(&manifest)
    return k.hash.Bytes(bz)
}

func (k Keeper) verifyBCIApproval(pubKey []byte, manifestHash []byte, proof *types.BCIChallengeSignature) bool {
    if proof == nil {
        return false
    }
    if len(pubKey) == 0 {
        return false
    }
    return k.bciVerifier.Verify(pubKey, manifestHash, proof.Signature)
}
