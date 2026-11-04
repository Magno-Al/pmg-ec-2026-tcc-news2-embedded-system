from preProcessingData.preProcessingData import PreProcessing
from machineLearningModels.XgBoost import TrainModel as XgBoost
from machineLearningModels.RandomForest import TrainModel as RandomForest


data = PreProcessing()

X_train, X_val, X_test, y_train, y_val, y_test = data.splitByPatient()

trainer = XgBoost(X_train, X_test, y_train, y_test)

trainer.train()

trainer.evaluate()

trainer.featureImportance()

rfTrainer = RandomForest(X_train, X_val, X_test, y_train, y_val, y_test)
rfTrainer.train()
rfTrainer.evaluate()
rfTrainer.featureImportance()

#X, y, groups = data.getTrainingMatrix()
#trainer.cross_validate_by_patient(X, y, groups)


'''
## Para aplicação de fine-tuning aos modelos de ML
from machineLearningModels.FineTuning.RandomSearch import RandomSearch

X_train, X_val, X_test, y_train, y_val, y_test = data.splitByPatient()

search = RandomSearch(X_train, X_val, y_train, y_val)
best_xgb, results_xgb = search.searchXgBoost()
best_rf, results_rf = search.searchRandomForest()
'''


