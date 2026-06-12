-- filename: data/lake_risk_az_co_system.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS lake_risk (
    lake_id                INTEGER PRIMARY KEY AUTOINCREMENT,
    lake_name              TEXT NOT NULL,
    state                  TEXT NOT NULL,
    basin_system           TEXT NOT NULL,
    primary_risk_type      TEXT NOT NULL,
    early_warning_signals  TEXT NOT NULL,
    management_agencies    TEXT NOT NULL,
    key_agency_contacts    TEXT NOT NULL,
    notes                  TEXT NOT NULL,
    source_refs            TEXT NOT NULL,
    bostrom_credit_did     TEXT NOT NULL
);

INSERT INTO lake_risk (
    lake_name,
    state,
    basin_system,
    primary_risk_type,
    early_warning_signals,
    management_agencies,
    key_agency_contacts,
    notes,
    source_refs,
    bostrom_credit_did
)
VALUES
-- 1. San Carlos Lake – post‑disaster baseline
(
    'San Carlos Lake',
    'AZ',
    'Gila River / San Carlos Irrigation Project',
    'Ecological collapse and public‑health risk from drought-driven fish kill',
    'Reservoir at very low storage (<5% capacity); sustained high air and surface-water temperatures; visible algal blooms and surface scum; dissolved oxygen dropping toward critical thresholds; continued mandatory releases despite low pool; field reports of stressed or dying fish near shorelines.',
    'San Carlos Recreation and Wildlife Department; San Carlos Apache Tribe; Bureau of Indian Affairs; San Carlos Irrigation and Drainage District.',
    'San Carlos Recreation & Wildlife public notices and Facebook page (official closure and fish‑kill updates); coordination via San Carlos Apache Tribal administration; emergency liaison with Arizona Game and Fish Department main line: 602‑942‑3000; AZGFD Region VI Mesa Office: 480‑981‑9400.',
    'In early June 2026, officials closed San Carlos Lake indefinitely after approximately 100% of the fish population died, driven by years of drought, extreme low water (around 1.9% capacity in early April 2026), high temperatures, algal growth, and continued water releases from Coolidge Dam for downstream irrigation. Decomposing fish created public‑health concerns, forcing the suspension of all fishing and related recreation. This lake is a reference case for what happens when early warnings about low water and oxygen stress are not paired with operational changes and rapid response.',
    'San Carlos official closure notices and media coverage of the 100% fish kill and indefinite closure in June 2026; Washington Times and People reports describing drought conditions, water releases from Coolidge Dam, and public‑health risks from decomposing fish.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),

-- 2. Lake Powell – structural water-supply and ecological risk
(
    'Lake Powell',
    'AZ/UT',
    'Colorado River – Upper/Lower Basin interface',
    'Low-reservoir structural risk (approach to minimum power pool) with associated ecological stress',
    'Projected or observed elevations dropping toward the minimum power pool (~3490–3500 ft); official Bureau of Reclamation 24‑month studies indicating heightened risk; extended hot, dry periods driving warm surface layers; increased residence time and coves with stagnant water; growing reports of harmful algal blooms or low dissolved oxygen near inlets and marinas.',
    'U.S. Bureau of Reclamation (Upper Colorado Region and Glen Canyon Dam operations); National Park Service (Glen Canyon National Recreation Area); state agencies in Arizona and Utah; tribal governments with Colorado River interests.',
    'Primary federal contact: U.S. Bureau of Reclamation communications office (Communications@usbr.gov); Lake Powell operations described in annual operating condition releases; for Arizona ecological coordination, Arizona Game and Fish Department main line 602‑942‑3000 and regional offices; local National Park Service resource staff at Glen Canyon National Recreation Area.',
    'Reclamation projections for water year 2026 place Lake Powell well below full pool and in a mid‑elevation operating tier, with elevation still above the minimum power pool but with limited buffer. If levels fall further, Glen Canyon Dam hydropower could be curtailed, affecting grid stability and funding for river management. Ecologically, lower and warmer water increases the risk of harmful algal blooms, stratification, and oxygen stress in embayments. Powell is system‑critical, so operational choices here ripple through to Lake Mead and all downstream habitats.',
    'U.S. Bureau of Reclamation operating condition releases for Lake Powell and Lake Mead for 2026, including elevation projections and shortage tier designations; regional reporting on long‑term drought and structural over‑allocation of the Colorado River.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),

-- 3. Lake Mead – chronic shortage and warming stress
(
    'Lake Mead',
    'AZ/NV',
    'Colorado River – Lower Basin',
    'Chronic low-reservoir levels, shortage conditions, and heat‑driven ecological stress',
    'Reservoir elevation in federally declared shortage tiers (for example Level 1 shortage around ~1055 ft); prolonged heat waves driving record high surface‑water temperatures; visible expansion of bathtub ring and dust‑exposed sediments; occurrence of harmful algal blooms or low‑oxygen events in coves and marinas; increased frequency of emergency operating agreements or shortage declarations.',
    'U.S. Bureau of Reclamation (Lower Colorado Region); Hoover Dam operators; National Park Service (Lake Mead National Recreation Area); Arizona, Nevada, and California state water agencies; tribal governments holding Colorado River entitlements.',
    'U.S. Bureau of Reclamation communications office (Communications@usbr.gov) for system‑level operations; Lake Mead National Recreation Area resource management contacts via NPS; for Arizona wildlife and fisheries issues, Arizona Game and Fish Department main line 602‑942‑3000 and regional contacts for western Arizona.',
    'Reclamation projections keep Lake Mead in a Level 1 shortage condition with elevations significantly below historical norms, reflecting the over‑allocated and warming Colorado River system. Low levels and high temperatures favor harmful algal blooms, reduced water quality, and stress on aquatic communities in near‑shore coves. Any operational failure at Mead would undermine water deliveries, hydropower production, and recreation for millions of people in the Lower Basin.',
    'U.S. Bureau of Reclamation shortage condition projections for Lake Mead around 2026; Colorado River drought analyses linking low elevations, shortage tiers, and increased ecological stress.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),

-- 4. Lake Havasu – intake contamination and recreation‑driven stress
(
    'Lake Havasu',
    'AZ/CA',
    'Lower Colorado River',
    'Water‑supply intake and recreation‑driven water‑quality risk',
    'Reports of harmful algal blooms or algal scums near drinking‑water intakes; unusual odors or discoloration in raw water; spikes in nutrient measurements downstream of urban areas or marinas; heavy holiday or weekend recreation with fuel and waste releases; regional drought and low‑flow periods reducing dilution capacity.',
    'U.S. Bureau of Reclamation (Lower Colorado projects); municipal and regional water providers drawing from Lake Havasu; Arizona Game and Fish Department for fisheries and wildlife; local city and county environmental health departments.',
    'Operations and environmental contacts via the Bureau of Reclamation Lower Colorado regional and Phoenix Area Offices; local water utility water‑quality contacts; Arizona Game and Fish Department main line 602‑942‑3000 and Region III (Kingman) office 928‑692‑7700 for western Arizona lakes.',
    'Lake Havasu is a key diversion point for major aqueducts serving central Arizona and southern California. While generally deeper and more stable than small inland lakes, it faces pressures from intense recreational use, upstream pollution, and broader Colorado River shortages. Water‑quality issues here would have immediate consequences for drinking‑water treatment, public health, and critical infrastructure.',
    'Lower Colorado River management documentation; regional water‑quality advisories for Lake Havasu and similar reservoirs; Arizona Game and Fish management information for western Arizona lakes.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),

-- 5. Roosevelt Lake – Salt River storage and bloom risk
(
    'Roosevelt Lake',
    'AZ',
    'Salt River Project – Salt River Basin',
    'Drought‑driven low storage with algal blooms and oxygen stress in coves',
    'Large drawdowns in drought years; high summer air temperatures and warm epilimnion; visible algal blooms in shallow arms and near inflow deltas; low dissolved oxygen measurements near the bottom and in back‑coves; rapid water‑level changes tied to flood control or irrigation releases.',
    'U.S. Bureau of Reclamation and Salt River Project (for dam and water‑supply operations); Arizona Game and Fish Department (for fisheries); local county health departments for recreation advisories.',
    'Salt River Project operations offices; Bureau of Reclamation Phoenix Area Office environmental staff; Arizona Game and Fish Department fisheries management contacts and regional offices, including Region VI Mesa Office 480‑981‑9400; AZGFD general line 602‑942‑3000 for reporting fish kills or bloom concerns.',
    'Roosevelt is a major storage and flood‑control reservoir for central Arizona. In dry years, substantial drawdowns can leave shallow zones warm and nutrient‑rich, fostering algal blooms and hypoxic events similar in mechanism to the San Carlos collapse, though on a different scale. Early monitoring of oxygen, temperature profiles, and bloom conditions in coves can provide lead time for operational adjustments and public advisories.',
    'Salt River Project and Reclamation histories of Roosevelt Lake operations; Arizona lake management guidance on harmful algal blooms and fish‑kill risks.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),

-- 6. Lake Pleasant – CAP‑linked level swings and heat stress
(
    'Lake Pleasant',
    'AZ',
    'Central Arizona Project / Agua Fria',
    'Operational water‑level swings with heat‑ and nutrient‑driven water‑quality risk',
    'Marked seasonal or operational changes in lake level related to Central Arizona Project operations; high summer surface temperatures coinciding with nutrient inputs from the watershed and recreation; observations of algal blooms, especially in shallow coves; reports of stressed fish or localized fish kills after heat waves and level changes.',
    'Maricopa County Parks and Recreation (park management); Central Arizona Project (for water operations); Arizona Game and Fish Department (for fisheries and wildlife); Maricopa County Environmental Services for public‑health advisories.',
    'Central Arizona Project operations contacts; Maricopa County Parks and Environmental Services offices; Arizona Game and Fish Department main line 602‑942‑3000 and regional contacts covering Lake Pleasant and the northwest Phoenix area.',
    'Lake Pleasant is a key storage and balancing reservoir for the Central Arizona Project and a heavily used recreation site for the Phoenix metro area. Fluctuating water levels, combined with high summer temperatures and nutrient inputs, create conditions favorable for algal blooms and localized oxygen depletion. Proactive monitoring and public communication can prevent recreation‑related health incidents and reduce the chance of San Carlos‑style surprise events.',
    'Regional water‑resources documentation on CAP and Lake Pleasant operations; Arizona recreation and water‑quality advisories for large urban‑adjacent reservoirs.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),

-- 7. Salt River canyon chain (Apache, Canyon, Saguaro Lakes) – recreation and flow variability
(
    'Apache, Canyon, and Saguaro Lakes (Salt River chain)',
    'AZ',
    'Salt River Project – Lower Salt River',
    'Combined risk from fluctuating flows, intense recreation, and localized blooms',
    'Rapid changes in inflows and releases from upstream dams; summer heat waves coinciding with high visitation; visible algal growth in quiet coves; reports of nuisance algal blooms or low oxygen in back‑waters; crowding and shoreline disturbance during holidays.',
    'Salt River Project and U.S. Bureau of Reclamation (for dam operations); Tonto National Forest (land management); Arizona Game and Fish Department; Maricopa County and local health agencies for advisories.',
    'Salt River Project river operations contacts; Tonto National Forest ranger districts; Arizona Game and Fish Department regional contacts (including Region VI Mesa Office 480‑981‑9400); AZGFD main line 602‑942‑3000 for reporting wildlife and fish issues.',
    'These linked canyon reservoirs provide critical recreation and habitat near the Phoenix area. Flow and level changes driven by upstream operations, combined with concentrated recreation and warming, increase the chance of localized fish stress and harmful algal blooms. Coordinated monitoring and rapid advisories can reduce ecological damage and protect public health.',
    'Salt River Project reservoir operations information; federal and state recreation advisories for the Salt River chain; AZGFD fisheries management guidance.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),

-- 8. High‑elevation small lakes (aggregate risk row)
(
    'High-elevation recreational lakes (aggregate row)',
    'AZ',
    'Headwater basins (e.g., White Mountains, Mogollon Rim)',
    'Temperature‑sensitivity of cold‑water fisheries and small‑volume lakes',
    'Drought‑driven low water levels; unusually warm summer temperatures at traditionally cool sites; reduced inflows from snowmelt; observations of stressed or dead cold‑water fish such as trout; emerging algae in normally clear, oligotrophic lakes; shoreline exposure of organic sediments.',
    'U.S. Forest Service (Apache‑Sitgreaves and Coconino National Forests); Arizona Game and Fish Department for stocking and fisheries; tribal and private landowners where lakes are not on federal land.',
    'Local Forest Service ranger districts; Arizona Game and Fish Department regional offices including Region I Pinetop (928‑367‑4281) and Region II Flagstaff (928‑774‑5045); AZGFD main line 602‑942‑3000 for general coordination.',
    'High‑elevation lakes across northern and eastern Arizona form a network of small, often shallow recreational fisheries that are highly sensitive to warming and drought. While each lake is smaller than San Carlos, cumulative fish kills or habitat loss across many such waters could significantly reduce regional cold‑water habitat. Early detection of warm‑water episodes and oxygen stress enables temporary stocking changes, targeted aeration where feasible, and revised recreation guidance.',
    'Forest Service and AZGFD information on high‑elevation lake management and trout fisheries; regional climate reports showing increasing heat stress at elevations that historically maintained cold‑water conditions.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);
