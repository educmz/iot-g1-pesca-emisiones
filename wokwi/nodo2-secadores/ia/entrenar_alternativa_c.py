#!/usr/bin/env python3
"""Alternativa C OPCIONAL: regresión logística propia con datos recogidos del simulador.
Uso: python ia/entrenar_alternativa_c.py datos_reales_del_grupo.csv
NO genera datos ficticios ni afirma entrenamiento sin muestras propias.
Predice un pico (gas >= 60% relativo) en las próximas 3 lecturas de 2 s.
Sólo una demostración analítica, no instrumento de seguridad industrial.
"""
import csv, json, math, sys
from pathlib import Path

THRESHOLD = 60.0
HORIZON = 3

def sigmoid(z):
    return 1.0/(1.0+math.exp(-max(-40.0,min(40.0,z))))

def load_rows(csv_path):
    with open(csv_path,newline='',encoding='utf-8-sig') as f:
        rows=list(csv.DictReader(f))
    out=[]
    for row in rows:
        try:
            if row['calidad'] not in ('OK','PRUEBA_INYECTADA'): continue
            out.append({'boot':row['boot'], 'ts':int(row['ts']), 'seq':int(row['seq']),
                        'gas':float(row['gas']), 'temp':float(row['temperatura']), 'hum':float(row['humedad'])})
        except (ValueError,KeyError,TypeError): continue
    return sorted(out,key=lambda r:(r['ts'],r['seq']))

def samples(rows):
    X=[];y=[]
    for i in range(2,len(rows)-HORIZON):
        recent=rows[i-2:i+1]
        future=rows[i+1:i+1+HORIZON]
        # Nunca mezclar lecturas de sesiones de arranque diferentes.
        if len({x['boot'] for x in recent+future})!=1: continue
        current=rows[i]
        X.append([current['gas'],current['temp'],current['hum'],
                  sum(x['gas'] for x in recent)/3.0,current['gas']-rows[i-2]['gas']])
        y.append(1 if max(x['gas'] for x in future)>=THRESHOLD else 0)
    return X,y

def metrics(y_true,y_prob):
    pred=[1 if p>=0.5 else 0 for p in y_prob]
    tp=sum(a==1 and b==1 for a,b in zip(y_true,pred))
    fp=sum(a==0 and b==1 for a,b in zip(y_true,pred))
    fn=sum(a==1 and b==0 for a,b in zip(y_true,pred))
    tn=sum(a==0 and b==0 for a,b in zip(y_true,pred))
    return {'TP':tp,'FP':fp,'FN':fn,'TN':tn,
            'precision':round(tp/max(1,tp+fp),3),'recall':round(tp/max(1,tp+fn),3),
            'falsas_alarmas':fp}

def train(X,y):
    n=len(X); cols=len(X[0]); cut=int(n*.75)
    Xtr,Xte=X[:cut],X[cut:]; ytr,yte=y[:cut],y[cut:]
    if len(Xtr)<30 or len(Xte)<10 or len(set(ytr))<2:
        raise ValueError('Datos insuficientes o de una sola clase en entrenamiento; genera picos y periodos normales con Wokwi.')
    avg=[sum(r[j] for r in Xtr)/cut for j in range(cols)]
    std=[max(1e-6,math.sqrt(sum((r[j]-avg[j])**2 for r in Xtr)/cut)) for j in range(cols)]
    def scale(data):return [[(r[j]-avg[j])/std[j] for j in range(cols)] for r in data]
    trainX,testX=scale(Xtr),scale(Xte)
    weights=[0.0]*(cols+1)
    positives=max(1,sum(ytr)); negatives=max(1,len(ytr)-positives)
    for epoch in range(750):
        grad=[0.0]*(cols+1)
        for row,label in zip(trainX,ytr):
            p=sigmoid(weights[0]+sum(w*z for w,z in zip(weights[1:],row)))
            cost_weight=(len(ytr)/(2*positives)) if label else (len(ytr)/(2*negatives))
            err=(p-label)*cost_weight
            grad[0]+=err
            for j,z in enumerate(row):grad[j+1]+=err*z
        rate=0.09
        for j in range(len(weights)):
            weights[j]-=rate*(grad[j]/len(ytr)+(0.002*weights[j] if j else 0))
    probability=lambda row:sigmoid(weights[0]+sum(w*z for w,z in zip(weights[1:],row)))
    pred=[probability(row) for row in testX]
    # Baseline A: disparar alarma solamente cuando el gas ya está >= umbral.
    baseline=[1.0 if row[0]>=THRESHOLD else 0.0 for row in Xte]
    return {'features':['gas_pct','temp_c','hum_pct','gas_media3','gas_pendiente2'],
            'media':avg,'desv':std,'pesos':weights,'train':len(Xtr),'test':len(Xte),
            'resultados_C':metrics(yte,pred),'baseline_A':metrics(yte,baseline),
            'advertencia':'Modelo experimental; no reemplaza umbrales locales ni controles industriales.'}

def main():
    if len(sys.argv)!=2:
        print('Uso: python ia/entrenar_alternativa_c.py <lecturas_exportadas.csv>');return 2
    rows=load_rows(sys.argv[1]);X,y=samples(rows)
    print(f'Filas válidas: {len(rows)}; ejemplos con horizonte: {len(y)}')
    if len(y)<60:
        print('No hay datos suficientes: recolecta >=60 ejemplos etiquetables del simulador.');return 1
    try:result=train(X,y)
    except ValueError as ex:print(ex);return 1
    dest=Path(sys.argv[1]).with_suffix('.modelo.json')
    dest.write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps(result,indent=2,ensure_ascii=False));print('Modelo guardado:',dest)
    return 0
if __name__=='__main__':raise SystemExit(main())
