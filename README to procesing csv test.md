# Taller 1 de estocasticos 


## Local Setup

Is recommended to use the git bash terminal on windows, or the terminal on linux.

1. Create a virtual environment, if you don't have one already

        py -m venv env-t1-estocasticos

2. Activate the virtual environment

    On Windows powershell

        .\meta-ads-web-scrapping\Scripts\activate

    On Windows using git bash

        source env-t1-estocasticos/Scripts/activate

    On linux

        source env-t1-estocasticos/bin/activate

3. To add the required packages to the requirements.txt file, run the following command

        pip freeze > requirements.txt

4. Install the dependencies

       py -m pip install -r requirements.txt

5. Exit the virtual environment

        deactivate

6. To run the project, you need to activate the virtual environment and stay in the root folder of the project, then run the following command

        py dataProcessing.py