from preProcessingData.preProcessingData import PreProcessing
from machineLearningModels.XgBoost import TrainModel



data = PreProcessing()

X_train, X_test, y_train, y_test = data.splitByPatient()

trainer = TrainModel(X_train, X_test, y_train, y_test)

trainer.train()

trainer.evaluate()

X, y, groups = data.getTrainingMatrix()
trainer.cross_validate_by_patient(X, y, groups)