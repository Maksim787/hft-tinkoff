import sys
import os

sys.path.append(os.getcwd())

import tinkoff.invest as inv
from scripts.common.utils import quotation_to_float, parse_config

# Config:
TICKER = 'SBER'
TICKER = 'TCSG'
ticker_type = 'share'
# ticker_type = 'etf'

config = parse_config()

with inv.Client(token=config.token) as client:
    for class_code in ['SPBXM', 'TQCB', 'TQBR', 'SPBHKEX',
                       'TQIR', 'TQTF', 'TQOB', 'TQOD',
                       'TQOY', 'SPBBND', 'SPBEQRU', 'SPHKTF_HKD',
                       'TQIF', 'SPHKTF_CNY', 'TQOE', 'PSAU',
                       'TQPI', 'TQTD', 'TQTE', 'SPBRUBND',
                       'SPBKZ', 'SPBRU', 'TQRD', 'TQIY']:
        try:
            if ticker_type == 'etf':
                response = client.instruments.etf_by(id_type=inv.InstrumentIdType.INSTRUMENT_ID_TYPE_TICKER, class_code=class_code, id=TICKER)
            elif ticker_type == 'share':
                response = client.instruments.share_by(id_type=inv.InstrumentIdType.INSTRUMENT_ID_TYPE_TICKER, class_code=class_code, id=TICKER)
            else:
                assert False, 'Unreachable'
            print()
            print(f'Found {class_code=}')
            print(f'Figi: {response.instrument.figi}')
            print(f'Lot size: {response.instrument.lot}')
            print(f'Px step: {quotation_to_float(response.instrument.min_price_increment)}')
            print(response)
            break
        except inv.exceptions.RequestError:
            print(f'Skip {class_code}')
