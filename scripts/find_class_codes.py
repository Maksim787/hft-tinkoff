import yaml
import tinkoff.invest as inv
from collections import Counter

with open('../private/config.yaml') as f:
    config = yaml.safe_load(f)
with inv.Client(token=config['runner']['token']) as client:
    codes = Counter([i.class_code for i in client.instruments.shares().instruments])
    codes += Counter([i.class_code for i in client.instruments.etfs().instruments])
    codes += Counter([i.class_code for i in client.instruments.bonds().instruments])
    print(codes)
    print([c[0] for c in codes.most_common(100)])
