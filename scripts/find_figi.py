import yaml
import tinkoff.invest as inv

with open('../private/config.yaml') as f:
    config = yaml.safe_load(f)

TICKER = "TMOS"

with inv.Client(token=config['runner']['token']) as client:
    for class_code in ['SPBXM', 'TQCB', 'TQBR', 'SPBHKEX',
                       'TQIR', 'TQTF', 'TQOB', 'TQOD',
                       'TQOY', 'SPBBND', 'SPBEQRU', 'SPHKTF_HKD',
                       'TQIF', 'SPHKTF_CNY', 'TQOE', 'PSAU',
                       'TQPI', 'TQTD', 'TQTE', 'SPBRUBND',
                       'SPBKZ', 'SPBRU', 'TQRD', 'TQIY']:
        try:
            response = client.instruments.etf_by(id_type=inv.InstrumentIdType.INSTRUMENT_ID_TYPE_TICKER, class_code=class_code, id=TICKER)
            print(f'Found {class_code}')
            print(f'Figi: {response.instrument.figi}')
            print(response)
            break
        except inv.exceptions.RequestError:
            print(f'Skip {class_code}')
