import yaml
from dataclasses import dataclass
from pathlib import Path

from tinkoff.invest.typedefs import AccountId


def quotation_to_float(q):
    return q.units + q.nano / 1e9


@dataclass
class Config:
    token: str
    account_id: AccountId


def parse_config() -> Config:
    config = read_config()
    return Config(
        token=config['runner']['token'],
        account_id=str(config['user']['account_id'])  # noqa
    )


def read_config() -> dict:
    config_path = Path.cwd()
    config_name = 'private/config.yaml'
    for i in range(10):
        if (config_path / config_name).exists():
            break
        config_path = config_path.parent
    else:
        assert False, 'Could not find config'
    with open(config_path / config_name, encoding='utf-8') as f:
        return yaml.safe_load(f)
