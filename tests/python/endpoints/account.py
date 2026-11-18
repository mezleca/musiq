from .common import Endpoint, authenticated

# account menu: https://github.com/SimoHypers/limusic/blob/f82fc1ec0fe1e12ea023cb6da1550a911aa80eaf/crates/innertube/src/endpoints.rs#L486
# account list: https://github.com/SimoHypers/limusic/blob/f82fc1ec0fe1e12ea023cb6da1550a911aa80eaf/crates/innertube/src/endpoints.rs#L517
class AccountEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        return super().validate(handler, request, query, raw) and authenticated(handler)

ENDPOINTS = (
    AccountEndpoint("POST", "/youtubei/v1/account/account_menu", "account-menu.json"),
    AccountEndpoint("POST", "/youtubei/v1/account/accounts_list", "accounts-list.json"),
)
