// filename: x/aimodel/keeper/msg_server_freeze.go
// destination: eco_restoration_shard/x/aimodel/keeper/msg_server_freeze.go
// repo-target: github.com/mk-bluebird/eco_restoration_shard

package keeper

import (
    "context"

    sdk "github.com/cosmos/cosmos-sdk/types"
    "github.com/mk-bluebird/eco_restoration_shard/x/aimodel/types"
)

func (k msgServer) FreezeAIModel(
    goCtx context.Context,
    msg *types.MsgFreezeAIModel,
) (*types.MsgFreezeAIModelResponse, error) {
    ctx := sdk.UnwrapSDKContext(goCtx)

    if err := k.verifySabotageProof(ctx, msg.ModelId, msg.ProofOfModelGuilt); err != nil {
        return nil, err
    }

    model, found := k.registry.GetModel(ctx, msg.ModelId)
    if !found {
        return nil, types.ErrModelNotFound
    }

    model.Status = types.ModelStatus_FROZEN
    k.registry.SetModel(ctx, model)

    if err := k.slashModelStake(ctx, model); err != nil {
        return nil, err
    }

    return &types.MsgFreezeAIModelResponse{}, nil
}

func (k Keeper) verifySabotageProof(
    ctx sdk.Context,
    modelID string,
    proof types.SabotageProof,
) error {
    if len(proof.SabotageTxHashes) == 0 {
        return types.ErrSabotageEvidenceMissing
    }

    if len(proof.ZkProofOfBias) == 0 {
        return types.ErrSabotageEvidenceMissing
    }

    if !k.zkVerifier.VerifyModelBiasProof(ctx, modelID, proof) {
        return types.ErrInvalidSabotageEvidence
    }

    return nil
}
