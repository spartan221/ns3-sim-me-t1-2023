import os
import numpy as np
import pandas as pd

# Process_csv

def process_single_csv(file_path):

    df = pd.read_csv(file_path, sep=',')
    processed_df = df.to_numpy()

    #Respuesta headers
    response = pd.DataFrame(columns=['Tiempo de respuesta', 'Notificador', 'Rescatista'])
    numLlamadasEfectivas = 0
    numLlamadasRealizadas = df['Type'].value_counts()['request']
    tiempoTotalSimulacion = processed_df[processed_df.shape[0] - 1][0]

    # Time     ,    Type,   Source,Destination, Bytes_sent
    # 0.0308614, request, 10.1.0.5,  10.1.0.11, 1000
    # 0.452074 ,   reply, 10.1.0.9,   10.1.0.5, 1000
    for i in range(processed_df.shape[0]):

        if i >= processed_df.shape[0] - 1:
            break

        if processed_df[i][1] == 'request':

            for j in range(i+1 , processed_df.shape[0]):

                if processed_df[j][1] == 'reply' and processed_df[i][2] == processed_df[j][3]:

                    numLlamadasEfectivas += 1

                    tiempoDeRespuesta = processed_df[j][0] - processed_df[i][0]

                    response = response._append({'Tiempo de respuesta': tiempoDeRespuesta, 'Notificador': processed_df[i][2], 'Rescatista': processed_df[j][2]}, ignore_index=True)

                    processed_df = np.delete(processed_df, j, 0)

                    break              
   
    return [response, numLlamadasEfectivas,  numLlamadasRealizadas - numLlamadasEfectivas, numLlamadasRealizadas, tiempoTotalSimulacion]

def process_csv_group(directory_path):

    csv_files = [f for f in os.listdir(directory_path) if f.endswith('.csv')]

    # Diccionario de dataframes
    dfs = {}

    for file in csv_files:

        nameProtocolFile = file.split("_")[1].split("-")[0]

        if dfs.get(nameProtocolFile) != None:

            data = process_single_csv(os.path.join(directory_path, file))

            dfs[nameProtocolFile][0][0] = dfs[nameProtocolFile][0][0]._append(data[0], ignore_index=True)
            dfs[nameProtocolFile][0][1] += data[1]
            dfs[nameProtocolFile][0][2] += data[2]
            dfs[nameProtocolFile][0][3] += data[3]
            dfs[nameProtocolFile][0][4] += data[4]

        else:

            dfs[nameProtocolFile] = [process_single_csv(os.path.join(directory_path, file))]

    return dfs

# Export Results to CSV

def exportCsvGroupDf(groupDataframesDict):

    for key, value in groupDataframesDict.items():
        value[0][0].to_csv('results/' + key + '_response.csv', index=False)

def exportCsvIndicatorsData(dataFrameIndicators):

    dataFrameIndicators.to_csv('results/indicadores.csv', index=False)

def createIndicators(groupDataframesDict):

    resultados = pd.DataFrame(
        columns=[
            'Protocolo',
            'Llamadas efectivas',
            'Porcentaje de llamadas efectivas',
            'Llamadas perdidas',
            'Porcentaje de llamadas perdidas',
            'Llamadas realizadas',
            'Tiempo total de simulacion',
            'Tiempo de respuesta promedio',
            'Tiempo de respuesta maximo',
            'Tiempo de respuesta minimo',
            'Tiempo de respuesta - mediana',
            'Tiempo de respuesta - desviacion estandar',
            'Tiempo de respuesta - varianza'
            ])


    for key, value in groupDataframesDict.items():
        
        resultados = resultados._append({
            'Protocolo': key, 
            'Llamadas efectivas': value[0][1], 
            'Porcentaje de llamadas efectivas': "%"+str((value[0][1]/value[0][3])*100),
            'Llamadas perdidas': value[0][2], 
            'Porcentaje de llamadas perdidas': "%"+str((value[0][2]/value[0][3])*100),
            'Llamadas realizadas': value[0][3],
            'Tiempo total de simulacion': value[0][4]},
            'Tiempo de respuesta promedio': value[0][0]['Tiempo de respuesta'].mean(),
            'Tiempo de respuesta maximo': value[0][0]['Tiempo de respuesta'].max(),
            'Tiempo de respuesta minimo':value[0][0]['Tiempo de respuesta'].min(),
            'Tiempo de respuesta - mediana': value[0][0]['Tiempo de respuesta'].median(),
            'Tiempo de respuesta - desviacion estandar' : value[0][0]['Tiempo de respuesta'].std(),
            'Tiempo de respuesta - varianza': value[0][0]['Tiempo de respuesta'].mvar(),
            ignore_index=True)

        if not os.path.exists('results'):
            os.makedirs('results')

    return resultados


def main():

    result = process_csv_group('tests')
    exportCsvGroupDf(result)
    indicators = createIndicators(result)
    exportCsvIndicatorsData(indicators)



    # resultados = pd.DataFrame(columns=['Protocolo', 'Llamadas efectivas', 'Llamadas perdidas', 'Llamadas realizadas', 'Tiempo total de simulacion'])

    # for key, value in result.items():
        
    #     resultados = resultados._append({'Protocolo': key, 'Llamadas efectivas': value[0][1], 'Llamadas perdidas': value[0][2], 'Llamadas realizadas': value[0][3], 'Tiempo total de simulacion': value[0][4]}, ignore_index=True)

    #     if not os.path.exists('results'):
    #         os.makedirs('results')

        #Exportar a csv el dataFrama
        #value[0][0].to_csv('results/' + key + '_response.csv', index=False)

    #resultados.to_csv('results/resultados.csv', index=False)

if __name__ == '__main__':
    main()