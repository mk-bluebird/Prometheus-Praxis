-- filename: db_eco_wealth_score.sql
-- destination: ecorestorationshard/db/db_eco_wealth_score.sql

PRAGMA foreign_keys = ON;

-- View: aggregate ecocredit balance from bank module.
CREATE VIEW IF NOT EXISTS v_bank_ecocredit_balance AS
SELECT
    b.address       AS host_address,
    SUM(CASE WHEN b.denom = 'ecocredit' THEN b.amount ELSE 0 END) AS ecocredit_balance
FROM bank_balances AS b
GROUP BY b.address;

-- View: aggregate EcoCampaignCredit per host.
CREATE VIEW IF NOT EXISTS v_campaign_ecocredit_balance AS
SELECT
    c.host_address,
    SUM(c.amount) AS ecocampaign_credit
FROM eco_campaign_credit AS c
GROUP BY c.host_address;

-- View: aggregate DataContributionCredit per host.
CREATE VIEW IF NOT EXISTS v_data_contribution_credit_balance AS
SELECT
    d.owner_address AS host_address,
    SUM(d.amount)   AS data_contribution_credit
FROM data_contribution_credit AS d
GROUP BY d.owner_address;

-- Unified EcoWealthScore view.
CREATE VIEW IF NOT EXISTS v_host_eco_wealth_score AS
SELECT
    h.host_address,
    COALESCE(b.ecocredit_balance, 0)          AS ecocredit_balance,
    COALESCE(c.ecocampaign_credit, 0)         AS ecocampaign_credit,
    COALESCE(d.data_contribution_credit, 0)   AS datacredit_balance,
    (
        COALESCE(b.ecocredit_balance, 0) +
        COALESCE(c.ecocampaign_credit, 0) +
        COALESCE(d.data_contribution_credit, 0)
    )                                         AS eco_wealth_score
FROM host_identity AS h
LEFT JOIN v_bank_ecocredit_balance AS b
    ON b.host_address = h.host_address
LEFT JOIN v_campaign_ecocredit_balance AS c
    ON c.host_address = h.host_address
LEFT JOIN v_data_contribution_credit_balance AS d
    ON d.host_address = h.host_address;
